/* Transform every scop of a linked AST and write the result out.
 *
 * ppcg reads one C source file and rewrites it: pet_transform_C_source
 * copies the text around each scop and puts generated code in place of
 * the scop itself.  A linked AST has no single file to rewrite -- the
 * functions it holds were written in as many units as were linked --
 * so this is a separate program rather than a mode of ppcg, in the way
 * that pet_linked_scop is separate from pet.
 *
 * What it does instead is rewrite each of those units.  A scop knows
 * the file it was written in, since its pet_loc names one, so the scops
 * are gathered, sorted by where they stand, and each unit is written
 * out with its own scops replaced.  The units keep their names, with
 * ".ppcg" put before the extension, and land in the directory given by
 * --output-dir.
 *
 * One output file per unit, and not one file for all of them: two units
 * of one program may each define a static function of the same name --
 * the corpus this is tested on does -- and a single file holding both
 * does not compile.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <isl/arg.h>
#include <isl/ctx.h>
#include <isl/id.h>
#include <isl/options.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/union_map.h>
#include <isl/union_set.h>

#include <pet.h>

#include "ast_link.h"

#include "cpu.h"
#include "ppcg.h"
#include "ppcg_options.h"

/* The same option tree ppcg has, so that the two run under the same
 * rules: ppcg reaches isl's own options through pet's child rather than
 * through one of its own, and a tree shaped differently gives isl a
 * different set of options than the one ppcg's code was written for.
 */
struct options {
	struct pet_options	*pet;
	struct ppcg_options	*ppcg;
	char			*output_dir;
};

ISL_ARGS_START(struct options, options_args)
ISL_ARG_CHILD(struct options, pet, "pet", &pet_options_args, "pet options")
ISL_ARG_CHILD(struct options, ppcg, NULL, &ppcg_options_args, "ppcg options")
ISL_ARG_STR(struct options, output_dir, 0, "output-dir", "dir", ".",
	"directory to write the transformed units to")
ISL_ARGS_END

ISL_ARG_DEF(options, struct options, options_args)

/* The scops found in the linked AST, in the order they were found.
 */
struct collected {
	struct pet_scop	**scop;
	int		n;
	int		size;
};

/* Callback for pet_linked_ast_foreach_scop: hold on to "scop".
 *
 * The callback takes ownership, and every scop is wanted: which unit
 * each one belongs to is only known once they have all been seen.
 */
static int collect(__isl_take struct pet_scop *scop, void *user)
{
	struct collected *c = user;

	if (c->n >= c->size) {
		int size = c->size ? 2 * c->size : 16;
		struct pet_scop **scops;

		scops = realloc(c->scop, size * sizeof(struct pet_scop *));
		if (!scops) {
			pet_scop_free(scop);
			return -1;
		}
		c->scop = scops;
		c->size = size;
	}

	c->scop[c->n++] = scop;

	return 0;
}

static void collected_free(struct collected *c)
{
	int i;

	for (i = 0; i < c->n; ++i)
		pet_scop_free(c->scop[i]);
	free(c->scop);
}

/* Copy the bytes of "input" from "start" up to "end" to "output".
 * If "end" is negative, copy up to the end of the file.
 *
 * This is what pet does between the scops of one file; the linked
 * driver does it between the scops of each file it writes out.
 */
static int copy_range(FILE *input, FILE *output, long start, long end)
{
	char buffer[1024];
	size_t n, m;

	if (end < 0) {
		if (fseek(input, 0, SEEK_END) < 0)
			return -1;
		end = ftell(input);
	}

	if (fseek(input, start, SEEK_SET) < 0)
		return -1;

	while (start < end) {
		n = end - start;
		if (n > sizeof(buffer))
			n = sizeof(buffer);
		n = fread(buffer, 1, n, input);
		if (n == 0)
			return -1;
		m = fwrite(buffer, 1, n, output);
		if (n != m)
			return -1;
		start += n;
	}

	return 0;
}

/* Order two scops by where they stand in their file.
 */
static int by_start(const void *a, const void *b)
{
	struct pet_scop *const *sa = a;
	struct pet_scop *const *sb = b;
	unsigned ua = pet_loc_get_start((*sa)->loc);
	unsigned ub = pet_loc_get_start((*sb)->loc);

	return ua < ub ? -1 : ua > ub ? 1 : 0;
}

/* Does "set" stand for an array the program itself has?
 *
 * pet names the arrays it invents, and tells its own apart by the name:
 * see pet_id_is_arg_or_ret, which asks the same question of __pet_arg
 * and __pet_ret.  A condition that was encapsulated is written to a
 * __pet_test of the same kind.  The ValueDecl behind an id would be a
 * better thing to ask, but it does not survive as far as here: every
 * array reaching this point, invented or not, carries an id with nothing
 * behind it.
 */
static isl_stat note_real_write(__isl_take isl_set *set, void *user)
{
	int *real = user;
	isl_id *id;
	const char *name;

	id = isl_set_get_tuple_id(set);
	name = id ? isl_id_get_name(id) : NULL;
	if (name && strncmp(name, "__pet_test", 10) != 0)
		*real = 1;
	isl_id_free(id);
	isl_set_free(set);

	return isl_stat_ok;
}

/* Is everything "scop" writes an array pet made up?
 *
 * Encapsulating dynamic control turns a condition into a write to a
 * variable pet invents, so that what the control depended on can be
 * spoken about.  Where the control it belongs to is inside the scop,
 * that is an intermediate step and the code that comes out says nothing
 * about it.  Where the scop is the encapsulation and nothing else --
 * autodetection over a real function finds these, a single break in the
 * middle of a loop being enough -- the code that comes out is a write to
 * a variable that appears nowhere in the program, in place of the
 * control it stood for.  It neither compiles nor means what was written.
 *
 * Such a scop is left as it was written.  There is nothing in it to
 * generate: what it describes is not a computation the program does, it
 * is what pet needed in order to describe a computation elsewhere.
 */
static int writes_nothing_real(struct pet_scop *scop)
{
	isl_union_set *written;
	int real = 0;

	written = isl_union_map_range(pet_scop_get_may_writes(scop));
	if (!written)
		return 0;
	if (isl_union_set_foreach_set(written, &note_real_write, &real) < 0)
		real = 1;
	isl_union_set_free(written);

	return !real;
}

/* Make "path" and every directory above it, as far as they are missing.
 *
 * Used to place a unit's output under a directory of its own, so it is
 * the caller of output_name that has to have made the way to it.
 */
static int make_way(char *path)
{
	char *slash;

	for (slash = path + 1; (slash = strchr(slash, '/')); ++slash) {
		*slash = '\0';
		if (mkdir(path, 0777) < 0 && errno != EEXIST) {
			fprintf(stderr, "unable to make %s\n", path);
			*slash = '/';
			return -1;
		}
		*slash = '/';
	}

	return 0;
}

/* Name the file the transformed "unit" is written to: its own name with
 * ".ppcg" put before the extension, under "dir", and under as much of
 * the unit's own path as it was given.
 *
 * The path is kept and not just the last part of it.  A program of any
 * size has two units of one name -- llama-dspark has a quants.c and a
 * repack.cpp twice over, once for the machine it is built for and once
 * for the plain version -- and naming the output after the last part
 * alone puts both in one file, where the second silently replaces the
 * first.  Two units of the corpus would then never reach the compiler
 * and what was built would not be the program that was read.
 */
static int output_name(char *out, size_t size, const char *dir,
	const char *unit)
{
	const char *ext;
	size_t len;

	ext = strrchr(unit, '.');
	if (ext && strchr(ext, '/'))
		ext = NULL;
	len = ext ? (size_t) (ext - unit) : strlen(unit);

	while (*unit == '/')
		++unit, --len;

	snprintf(out, size, "%s/%.*s.ppcg%s", dir, (int) len, unit,
		ext ? ext : ".c");

	return make_way(out);
}

/* Write "unit" out with the scops in "scop" replaced by generated code.
 *
 * The scops are the ones that were written in this unit, already in the
 * order they stand in it.  Between them, and before the first and after
 * the last, the text of the unit is copied over as it was written.
 *
 * A scop that no code comes back for leaves its own text standing.  Not
 * every scop a linked AST holds can be scheduled -- an array whose
 * extent cannot be determined is enough -- and a unit is a whole file
 * that has to compile, so one scop is not worth the other thousand
 * lines around it.  Abandoning the file there is what used to happen,
 * and it left the unit cut off in the middle of a function: 89 lines
 * written of ggml-alloc.c's 1248, ending inside the body of the
 * function whose scop had failed.
 *
 * Each scop is printed to a string first for that reason.  Printing
 * straight to the file would have put whatever the printer managed
 * before it gave up in front of the text that then has to replace it.
 */
static int transform_unit(isl_ctx *ctx, const char *unit,
	struct pet_scop **scop, int n, struct ppcg_options *options,
	const char *dir)
{
	char name[4096];
	FILE *in, *out;
	long end = 0;
	int i, r = 0;
	int left = 0;

	in = fopen(unit, "r");
	if (!in) {
		fprintf(stderr, "unable to read %s\n", unit);
		return -1;
	}

	if (output_name(name, sizeof(name), dir, unit) < 0) {
		fclose(in);
		return -1;
	}
	out = fopen(name, "w");
	if (!out) {
		fprintf(stderr, "unable to write %s\n", name);
		fclose(in);
		return -1;
	}

	for (i = 0; i < n; ++i) {
		unsigned start = pet_loc_get_start(scop[i]->loc);
		unsigned stop = pet_loc_get_end(scop[i]->loc);
		isl_printer *p;
		char *text;

		if (copy_range(in, out, end, start) < 0) {
			r = -1;
			break;
		}

		p = isl_printer_to_str(ctx);
		p = isl_printer_set_output_format(p, ISL_FORMAT_C);
		p = isl_printer_set_indent_prefix(p,
					pet_loc_get_indent(scop[i]->loc));
		if (writes_nothing_real(scop[i])) {
			isl_printer_free(p);
			p = NULL;
			pet_scop_free(scop[i]);
		} else {
			p = ppcg_transform_scop(p, scop[i], options,
						&print_cpu_wrap, options);
		}
		scop[i] = NULL;
		text = p ? isl_printer_get_str(p) : NULL;
		isl_printer_free(p);

		if (text) {
			size_t len = strlen(text);

			if (fwrite(text, 1, len, out) != len)
				r = -1;
			free(text);
			if (r < 0)
				break;
		} else {
			fprintf(stderr, "%s: nothing came back for the scop "
				"at %u; it is left as it was written\n",
				unit, start);
			++left;
			if (copy_range(in, out, start, stop) < 0) {
				r = -1;
				break;
			}
		}
		end = stop;
	}

	if (left)
		fprintf(stderr, "%s: %d scop(s) left as written\n",
			unit, left);

	if (r == 0 && copy_range(in, out, end, -1) < 0)
		r = -1;

	fclose(out);
	fclose(in);

	if (r == 0)
		printf("%s\n", name);

	return r;
}

/* Write out every unit that holds a scop.
 *
 * The scops are grouped by the file they were written in, which their
 * pet_loc names, and each group is sorted by where it stands.
 *
 * A scop that does not say where it was written is passed over.  There
 * is no unit to put it in and nothing to be done about it here, and it
 * used to stop the run: over 47 units of llama-dspark one such scop
 * meant that none of the other units were written out at all, after
 * sixteen minutes of work on them.  How many there were is said at the
 * end, since it is a thing worth knowing and not a thing worth losing
 * the run over.
 */
static int transform_units(isl_ctx *ctx, struct collected *c,
	struct ppcg_options *options, const char *dir)
{
	int i, j;
	int r = 0;
	int placeless = 0;

	for (i = 0; i < c->n && r == 0; ++i) {
		char *unit;
		struct pet_scop **group;
		int n = 0;

		if (!c->scop[i])
			continue;
		if (!pet_loc_get_filename(c->scop[i]->loc)) {
			++placeless;
			pet_scop_free(c->scop[i]);
			c->scop[i] = NULL;
			continue;
		}
		unit = strdup(pet_loc_get_filename(c->scop[i]->loc));
		group = calloc(c->n, sizeof(struct pet_scop *));
		if (!unit || !group) {
			free(unit);
			free(group);
			r = -1;
			break;
		}

		/* Every scop written in this unit, taken out of the
		 * collection as it is put in the group: what goes into
		 * transform_unit is handed over to it.
		 */
		for (j = i; j < c->n; ++j) {
			const char *f;

			if (!c->scop[j])
				continue;
			f = pet_loc_get_filename(c->scop[j]->loc);
			if (f && !strcmp(f, unit)) {
				group[n++] = c->scop[j];
				c->scop[j] = NULL;
			}
		}
		qsort(group, n, sizeof(struct pet_scop *), &by_start);

		r = transform_unit(ctx, unit, group, n, options, dir);

		free(group);
		free(unit);
	}

	if (placeless)
		fprintf(stderr, "%d scop(s) did not say which file they were "
			"written in and were passed over\n", placeless);

	return r;
}

int main(int argc, char **argv)
{
	isl_ctx *ctx;
	struct options *options;
	struct pet_linked_ast *linked;
	struct collected c = { NULL, 0, 0 };
	char **units;
	int n, n_units;
	int r;

	/* The units to link are however many are given, which the option
	 * parser has no way of describing, so they are separated from the
	 * options by hand: everything from the first argument that is not
	 * an option onwards is a unit.
	 */
	for (n = 1; n < argc; ++n)
		if (argv[n][0] != '-')
			break;
	units = argv + n;
	n_units = argc - n;
	argc = n;

	options = options_new_with_defaults();
	if (!options)
		return EXIT_FAILURE;
	ctx = isl_ctx_alloc_with_options(&options_args, options);

	/* The same defaults ppcg's own main() forces, or a scop that
	 * came through the link is scheduled and printed under other
	 * rules than the same scop read from its own file.
	 */
	/* CPU is the only target this driver generates for, and the target
	 * decides which dependences are computed: with the default target
	 * left as it is, compute_dependences takes the tagged branch and
	 * dep_flow stays empty, so the schedule comes back as nothing.
	 */
	options->ppcg->target = PPCG_TARGET_C;
	ppcg_options_set_target_defaults(options->ppcg);
	isl_options_set_ast_build_detect_min_max(ctx, 1);
	isl_options_set_ast_print_macro_once(ctx, 1);
	isl_options_set_schedule_whole_component(ctx, 0);
	isl_options_set_schedule_maximize_band_depth(ctx, 1);
	isl_options_set_schedule_maximize_coincidence(ctx, 1);
	pet_options_set_encapsulate_dynamic_control(ctx, 1);

	argc = options_parse(options, argc, argv, ISL_ARG_ALL);

	/* Pragmas are consumed when a unit is serialised and are not in
	 * the AST, so a linked AST is only ever autodetected.
	 */
	pet_options_set_autodetect(ctx, 1);

	/* Asked for another target on the command line, it is still CPU
	 * that comes out, so the option is not left saying otherwise.
	 */
	if (options->ppcg->target != PPCG_TARGET_C) {
		options->ppcg->target = PPCG_TARGET_C;
		ppcg_options_set_target_defaults(options->ppcg);
	}

	if (n_units < 1) {
		fprintf(stderr, "%s: no units to link\n", argv[0]);
		isl_ctx_free(ctx);
		return EXIT_FAILURE;
	}

	linked = pet_ast_link((const char **) units, n_units);
	if (!linked) {
		fprintf(stderr, "%s: cannot link\n", argv[0]);
		isl_ctx_free(ctx);
		return EXIT_FAILURE;
	}
	if (pet_linked_ast_n_refused(linked) != 0) {
		int n_refused = pet_linked_ast_n_refused(linked);

		/* A refusal costs a chance and not the program.  What is
		 * refused is a declaration that did not become one entity
		 * with the target's, and what depends on that is whether a
		 * call can find the body it names: found, the body is put
		 * in place of the call; not found, the call stays a call,
		 * which is what the source said in the first place.  The
		 * code that comes out is the program that was given either
		 * way, transformed in fewer places.
		 *
		 * It was fatal here, on the grounds that a declaration
		 * which did not cross means the program generated is not
		 * the program given.  Over C++ that stops everything and
		 * buys nothing: linking llama-dspark refuses 14124
		 * declarations, nearly all of them the standard library's
		 * templates seen from more than one unit, and not one of
		 * them is a body a scop was going to reach.
		 *
		 * The names are behind PPCG_LINK_REFUSED, as pet puts its
		 * own account of the link behind PET_LINK_WHY: fourteen
		 * thousand lines of them is not a thing to print at
		 * somebody who asked for code.
		 */
		fprintf(stderr, "%s: %d declaration(s) could not be linked; "
			"calls to them are left as calls\n",
			argv[0], n_refused);
		if (getenv("PPCG_LINK_REFUSED")) {
			int i;

			for (i = 0; i < n_refused; ++i)
				fprintf(stderr, "  %s: %s\n",
					pet_linked_ast_refused(linked, i),
					pet_linked_ast_refused_why(linked, i));
		}
	}

	r = pet_linked_ast_foreach_scop(ctx, linked, &collect, &c);
	if (r == 0 && c.n == 0) {
		fprintf(stderr, "%s: no scop found in any of the %d unit(s)\n",
			argv[0], n_units);
		r = -1;
	}
	if (r == 0)
		r = transform_units(ctx, &c, options->ppcg,
					options->output_dir);

	collected_free(&c);
	pet_ast_link_free(linked);
	isl_ctx_free(ctx);

	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
