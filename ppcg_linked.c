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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isl/arg.h>
#include <isl/ctx.h>
#include <isl/options.h>
#include <isl/printer.h>

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

/* Name the file the transformed "unit" is written to: its own name with
 * ".ppcg" put before the extension, inside "dir".
 */
static void output_name(char *out, size_t size, const char *dir,
	const char *unit)
{
	const char *base, *ext;
	size_t len;

	base = strrchr(unit, '/');
	base = base ? base + 1 : unit;
	ext = strrchr(base, '.');
	len = ext ? (size_t) (ext - base) : strlen(base);

	snprintf(out, size, "%s/%.*s.ppcg%s", dir, (int) len, base,
		ext ? ext : ".c");
}

/* Write "unit" out with the scops in "scop" replaced by generated code.
 *
 * The scops are the ones that were written in this unit, already in the
 * order they stand in it.  Between them, and before the first and after
 * the last, the text of the unit is copied over as it was written.
 */
static int transform_unit(isl_ctx *ctx, const char *unit,
	struct pet_scop **scop, int n, struct ppcg_options *options,
	const char *dir)
{
	char name[4096];
	FILE *in, *out;
	isl_printer *p;
	long end = 0;
	int i, r = 0;

	in = fopen(unit, "r");
	if (!in) {
		fprintf(stderr, "unable to read %s\n", unit);
		return -1;
	}

	output_name(name, sizeof(name), dir, unit);
	out = fopen(name, "w");
	if (!out) {
		fprintf(stderr, "unable to write %s\n", name);
		fclose(in);
		return -1;
	}

	p = isl_printer_to_file(ctx, out);
	p = isl_printer_set_output_format(p, ISL_FORMAT_C);

	for (i = 0; i < n; ++i) {
		unsigned start = pet_loc_get_start(scop[i]->loc);

		if (copy_range(in, out, end, start) < 0) {
			r = -1;
			break;
		}
		end = pet_loc_get_end(scop[i]->loc);
		p = isl_printer_set_indent_prefix(p,
					pet_loc_get_indent(scop[i]->loc));
		p = ppcg_transform_scop(p, scop[i], options,
					&print_cpu_wrap, options);
		scop[i] = NULL;
		if (!p) {
			fprintf(stderr, "%s: nothing came back for the scop "
				"at %u\n", unit, start);
			r = -1;
			break;
		}
	}

	if (r == 0 && copy_range(in, out, end, -1) < 0)
		r = -1;

	isl_printer_free(p);
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
 */
static int transform_units(isl_ctx *ctx, struct collected *c,
	struct ppcg_options *options, const char *dir)
{
	int i, j;
	int r = 0;

	for (i = 0; i < c->n && r == 0; ++i) {
		char *unit;
		struct pet_scop **group;
		int n = 0;

		if (!c->scop[i])
			continue;
		if (!pet_loc_get_filename(c->scop[i]->loc)) {
			fprintf(stderr, "a scop does not say which file it "
				"was written in; the units have to be "
				"serialised by pet_emit_ast\n");
			r = -1;
			break;
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
		int i, n_refused = pet_linked_ast_n_refused(linked);

		fprintf(stderr, "%s: %d declaration(s) could not be linked\n",
			argv[0], n_refused);
		for (i = 0; i < n_refused; ++i)
			fprintf(stderr, "  %s: %s\n",
				pet_linked_ast_refused(linked, i),
				pet_linked_ast_refused_why(linked, i));
		/* A refusal is fatal here, unlike in a map: the code that
		 * comes out is compiled and run, and a declaration that did
		 * not cross means the program that is generated is not the
		 * program that was given.  Saying so beats writing out
		 * something that quietly computes something else.
		 */
		pet_ast_link_free(linked);
		isl_ctx_free(ctx);
		return EXIT_FAILURE;
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
