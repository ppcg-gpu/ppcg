#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isl/aff.h>
#include <isl/id.h>
#include <isl/multi.h>
#include <isl/space.h>
#include <isl/flow.h>
#include <isl/ilp.h>
#include <isl/schedule.h>
#include <isl/set.h>
#include <isl/union_set.h>
#include <isl/val.h>

#include "reduction.h"

/* Is "op" an operator whose repeated application gives a result that does
 * not depend on the order of the operands?
 *
 * Division and subtraction are compound assignments too, but x -= a
 * followed by x -= b is not the same as applying them the other way
 * round unless the accumulator is treated as a sum of negated terms,
 * which is a rewrite rather than a relaxation, so they are left out.
 *
 * Floating point addition and multiplication are not associative either.
 * They are accepted here because that is the trade every compiler makes
 * for a reduction, and because refusing them would leave the case this
 * is meant to address, a sum over an array of floats, unhandled.
 */
static int op_is_associative(enum pet_op_type op)
{
	switch (op) {
	case pet_op_add_assign:
	case pet_op_mul_assign:
	case pet_op_and_assign:
	case pet_op_or_assign:
	case pet_op_xor_assign:
		return 1;
	default:
		return 0;
	}
}

/* Do "a" and "b" access the same location?
 *
 * Both are index expressions of accesses to the same array, so they
 * describe the same location exactly when they are the same function of
 * the iteration vector.
 */
static int same_location(__isl_keep isl_multi_pw_aff *a,
	__isl_keep isl_multi_pw_aff *b)
{
	isl_bool equal;

	equal = isl_multi_pw_aff_plain_is_equal(a, b);

	return equal == isl_bool_true;
}

/* Release what "red" holds, leaving it as empty as it started out.
 */
static void reduction_clear(struct ppcg_reduction *red)
{
	isl_id_free(red->ref);
	isl_multi_pw_aff_free(red->index);
	red->ref = NULL;
	red->index = NULL;
}

/* Is "expr" a compound assignment that accumulates into the location it
 * reads?  If so, describe it in "red".
 *
 * The first argument of a compound assignment is the accumulator, both
 * read and written.  pet represents it as a single access expression that
 * is marked as both, so the check is that this argument is an access, that
 * it is read as well as written, and that the operator is one whose order
 * does not matter.
 *
 * A member of a structure is refused.  pet gives the field an array of
 * its own, named after the structure and the field together, and that
 * name is not one the generated code declares, so it could not be put in
 * a clause.  Such an access is the one whose index expression has a
 * wrapped range, the structure on one side and the field on the other.
 */
static int extract_reduction(__isl_keep pet_expr *expr, int stmt,
	struct ppcg_reduction *red)
{
	enum pet_op_type op;
	pet_expr *arg;
	isl_bool is_read, is_write;
	isl_space *space;
	int ok = 0;

	if (pet_expr_get_type(expr) != pet_expr_op)
		return 0;
	op = pet_expr_op_get_type(expr);
	if (!op_is_associative(op))
		return 0;
	if (pet_expr_get_n_arg(expr) < 1)
		return 0;

	arg = pet_expr_get_arg(expr, 0);
	if (!arg)
		return 0;
	if (pet_expr_get_type(arg) != pet_expr_access) {
		pet_expr_free(arg);
		return 0;
	}
	is_read = pet_expr_access_is_read(arg);
	is_write = pet_expr_access_is_write(arg);
	if (is_read == isl_bool_true && is_write == isl_bool_true) {
		red->stmt = stmt;
		red->op = op;
		red->ref = pet_expr_access_get_ref_id(arg);
		red->index = pet_expr_access_get_index(arg);
		ok = red->ref != NULL && red->index != NULL;
	}
	if (ok) {
		space = isl_space_range(isl_multi_pw_aff_get_space(red->index));
		ok = isl_space_is_wrapping(space) != isl_bool_true;
		isl_space_free(space);
		if (!ok)
			reduction_clear(red);
	}
	pet_expr_free(arg);

	return ok;
}

/* Count, in "user", the accesses of a statement that matter to whether
 * it is an accumulation and nothing else.
 */
struct reduction_accesses {
	/* The array the accumulator belongs to. */
	isl_id *array;
	/* Accesses to that array. */
	int on_accumulator;
	/* Accesses that write. */
	int writes;
};

static int count_access(__isl_keep pet_expr *expr, void *user)
{
	struct reduction_accesses *count = user;
	isl_id *id;

	id = pet_expr_access_get_id(expr);
	if (id == count->array)
		count->on_accumulator++;
	isl_id_free(id);

	if (pet_expr_access_is_write(expr) == isl_bool_true)
		count->writes++;

	return 0;
}

/* Is the statement "tree" performs nothing but the accumulation "red"?
 *
 * The dependences that are dropped for an accumulation are the ones
 * between the iterations that accumulate into the same location, and
 * they are dropped on the grounds that every one of them is an
 * accumulation carried by that location.  That only holds when the
 * statement touches the accumulator once and writes nothing else:
 *
 *	s += s * 0.5f + a[i];
 *
 * reads the accumulator a second time, which makes each iteration
 * depend on the value the previous one left rather than merely add to
 * it, and
 *
 *	h[i % B] += a[i] * b[i % B]++;
 *
 * carries a dependence through b between the very iterations whose
 * ordering would be dropped, leaving b to be updated by several threads
 * at once.  Neither is an accumulation, whatever the operator says.
 *
 * The condition of an if is part of the statement and is counted too, so
 * a condition that reads the accumulator, as in
 *
 *	if (s < 100.0f)
 *		s += a[i];
 *
 * is refused here as well: whether an iteration accumulates would depend
 * on how much has been accumulated so far.
 */
static int only_accumulates(__isl_keep pet_tree *tree,
	struct ppcg_reduction *red)
{
	struct reduction_accesses count = { NULL, 0, 0 };
	int ok;

	count.array = isl_multi_pw_aff_get_tuple_id(red->index, isl_dim_out);
	if (!count.array)
		return 0;
	ok = pet_tree_foreach_access_expr(tree, &count_access, &count) >= 0;
	isl_id_free(count.array);

	return ok && count.on_accumulator == 1 && count.writes == 1;
}

/* Look for an accumulation in "tree".
 *
 * An accumulation written under a condition of its own, as in
 *
 *	if (a[i] > 0)
 *		sum += a[i];
 *
 * is still one: the iterations that do accumulate may be run in any
 * order, and the ones that do not are no obstacle to that.  pet keeps
 * such an if together with what it guards in a single statement, so the
 * condition is looked through.
 */
static int find_in_tree(__isl_keep pet_tree *tree, int pos,
	struct ppcg_reduction *red)
{
	pet_expr *expr;
	pet_tree *then;
	int found;

	switch (pet_tree_get_type(tree)) {
	case pet_tree_expr:
		expr = pet_tree_expr_get_expr(tree);
		if (!expr)
			return 0;
		found = extract_reduction(expr, pos, red);
		pet_expr_free(expr);
		return found;
	case pet_tree_if:
		then = pet_tree_if_get_then(tree);
		if (!then)
			return 0;
		found = find_in_tree(then, pos, red);
		pet_tree_free(then);
		return found;
	default:
		return 0;
	}
}

/* Look for an accumulation in the body of statement "stmt".
 *
 * A statement only counts as one when the accumulation is all it does,
 * so what was found is checked against the accesses of the whole
 * statement before it is accepted.
 */
static int find_in_stmt(struct pet_stmt *stmt, int pos,
	struct ppcg_reduction *red)
{
	if (!find_in_tree(stmt->body, pos, red))
		return 0;
	if (only_accumulates(stmt->body, red))
		return 1;

	reduction_clear(red);

	return 0;
}

struct ppcg_reductions *ppcg_find_reductions(struct ppcg_scop *scop)
{
	struct ppcg_reductions *reductions;
	int i;

	if (!scop)
		return NULL;

	reductions = calloc(1, sizeof(*reductions));
	if (!reductions)
		return NULL;
	reductions->red = calloc(scop->pet->n_stmt, sizeof(*reductions->red));
	if (scop->pet->n_stmt && !reductions->red) {
		free(reductions);
		return NULL;
	}

	for (i = 0; i < scop->pet->n_stmt; ++i) {
		struct ppcg_reduction red = { 0 };

		if (!find_in_stmt(scop->pet->stmts[i], i, &red))
			continue;
		reductions->red[reductions->n++] = red;
	}

	return reductions;
}

void ppcg_reductions_free(struct ppcg_reductions *reductions)
{
	int i;

	if (!reductions)
		return;
	for (i = 0; i < reductions->n; ++i)
		reduction_clear(&reductions->red[i]);
	free(reductions->red);
	free(reductions);
}

static const char *op_str(enum pet_op_type op)
{
	switch (op) {
	case pet_op_add_assign: return "+";
	case pet_op_mul_assign: return "*";
	case pet_op_and_assign: return "&";
	case pet_op_or_assign: return "|";
	case pet_op_xor_assign: return "^";
	default: return "?";
	}
}

void ppcg_reductions_print(FILE *out, struct ppcg_scop *scop,
	struct ppcg_reductions *reductions)
{
	int i;

	if (!reductions)
		return;

	for (i = 0; i < reductions->n; ++i) {
		struct ppcg_reduction *red = &reductions->red[i];
		isl_space *space;
		const char *name;

		space = isl_multi_pw_aff_get_space(red->index);
		name = isl_space_get_tuple_name(space, isl_dim_out);
		fprintf(out, "reduction %s %s\n", name ? name : "?",
			op_str(red->op));
		isl_space_free(space);
	}
	fprintf(out, "reductions %d of %d statements\n", reductions->n,
		scop->pet->n_stmt);
}

int ppcg_reduction_is_scalar(struct ppcg_reduction *red)
{
	return isl_multi_pw_aff_dim(red->index, isl_dim_out) == 0;
}

const char *ppcg_reduction_name(struct ppcg_reduction *red)
{
	isl_id *id;
	const char *name;

	id = isl_multi_pw_aff_get_tuple_id(red->index, isl_dim_out);
	name = id ? isl_id_get_name(id) : NULL;
	isl_id_free(id);

	return name;
}

const char *ppcg_reduction_op_str(struct ppcg_reduction *red)
{
	return op_str(red->op);
}

/* The accesses to the accumulator of "red", as a relation from the
 * iterations of the statement that performs it to the location it
 * accumulates into.
 */
static __isl_give isl_map *reduction_access(struct ppcg_reduction *red)
{
	return isl_map_from_multi_pw_aff(isl_multi_pw_aff_copy(red->index));
}

/* Append "suffix" to "s".  Both are freed, and NULL is returned, when
 * either of them is NULL or the result cannot be allocated, so that a
 * chain of these only has to be checked once, at the end.
 */
static char *append(char *s, char *suffix)
{
	size_t len;
	char *grown;

	if (!s || !suffix) {
		free(s);
		free(suffix);
		return NULL;
	}

	len = strlen(s);
	grown = realloc(s, len + strlen(suffix) + 1);
	if (grown)
		strcpy(grown + len, suffix);
	else
		free(s);
	free(suffix);

	return grown;
}

/* The pet_array of the array "red" accumulates into, or NULL when the
 * scop does not describe it.
 */
static struct pet_array *reduction_array(struct ppcg_scop *scop,
	struct ppcg_reduction *red)
{
	const char *name;
	int i;

	name = ppcg_reduction_name(red);
	if (!name)
		return NULL;

	for (i = 0; i < scop->pet->n_array; ++i) {
		const char *array;

		array = isl_set_get_tuple_name(scop->pet->arrays[i]->extent);
		if (array && !strcmp(array, name))
			return scop->pet->arrays[i];
	}

	return NULL;
}

/* Do the bounds of dimension "pos" of "set" have constant values, and if
 * so, which?
 */
static int constant_range(__isl_keep isl_set *set, int pos, long *lo,
	long *len)
{
	isl_val *min, *max;
	int ok;

	min = isl_set_dim_min_val(isl_set_copy(set), pos);
	max = isl_set_dim_max_val(isl_set_copy(set), pos);
	ok = isl_val_is_int(min) == isl_bool_true &&
		isl_val_is_int(max) == isl_bool_true;
	if (ok) {
		*lo = isl_val_get_num_si(min);
		*len = isl_val_get_num_si(max) - *lo + 1;
	}
	isl_val_free(min);
	isl_val_free(max);

	return ok;
}

/* Every thread is given a copy of the whole of the section, and the
 * copy is placed on that thread's stack, so a section of the size of a
 * stack crashes the generated program rather than speeding it up.  A
 * megabyte is an order of magnitude below the eight the default thread
 * stack has, and a section of that size still leaves the loop several
 * times faster than running it sequentially.
 */
#define PPCG_REDUCTION_MAX_BYTES	(1024 * 1024)

/* The section of the accumulator of "red" that the iterations in
 * "domain" accumulate into, as it is written in an OpenMP reduction
 * clause, or NULL when there is no such section to write.
 *
 * A clause names a section rather than a single element because which
 * element an iteration accumulates into is only known once the loop
 * runs.  Every thread is given a copy of the whole section and the
 * copies are added up afterwards, which is what makes the loop worth
 * running in parallel; an atomic update of the element instead leaves
 * the loop no faster than it was, since the accumulation is all the
 * loop does.
 *
 * There is nothing to write when
 *
 * - the loop reaches the same array by any other means, since within the
 *   loop the array stands for the copy, so an access that is not part of
 *   the accumulation would silently be redirected to it;
 * - which elements are accumulated into depends on a parameter.  The
 *   range would then have to be widened to cover every value the
 *   parameter may take, and the copies would be added back into elements
 *   the program never meant to have: h[i % 16] += a[i] with i below a
 *   parameter n reaches past the end of an h of n elements;
 * - the elements are not a constant range, or there are too many of
 *   them;
 * - the section would not be contiguous.  A section of an array of more
 *   than one dimension only is when it covers the whole of every
 *   dimension but the outermost.
 */
static char *reduction_section(struct ppcg_scop *scop,
	struct ppcg_reduction *red, __isl_keep isl_union_set *domain)
{
	struct pet_array *array;
	isl_map *acc;
	isl_union_map *other;
	isl_set *accessed;
	char *section;
	long elements = 1;
	int i, n;

	array = reduction_array(scop, red);
	if (!array || array->element_size <= 0)
		return NULL;

	acc = reduction_access(red);

	other = isl_union_map_union(isl_union_map_copy(scop->reads),
				isl_union_map_copy(scop->may_writes));
	other = isl_union_map_intersect_domain(other,
				isl_union_set_copy(domain));
	other = isl_union_map_intersect_range(other,
			isl_union_set_from_set(isl_set_universe(
				isl_space_range(isl_map_get_space(acc)))));
	other = isl_union_map_subtract(other,
				isl_union_map_from_map(isl_map_copy(acc)));
	if (isl_union_map_is_empty(other) != isl_bool_true) {
		isl_union_map_free(other);
		isl_map_free(acc);
		return NULL;
	}
	isl_union_map_free(other);

	/* Only what this loop runs, so that the section does not name
	 * elements another loop reaches.  The elements have to follow
	 * from the iterations alone: when a parameter has a say in which
	 * of them are accumulated into, the range isl can give covers
	 * every value that parameter may take, which is more elements
	 * than the program has.
	 */
	acc = isl_map_intersect_domain(acc,
			isl_set_from_union_set(isl_union_set_copy(domain)));
	if (isl_map_involves_dims(acc, isl_dim_param, 0,
				isl_map_dim(acc, isl_dim_param))) {
		isl_map_free(acc);
		return NULL;
	}

	accessed = isl_map_range(acc);
	n = isl_set_dim(accessed, isl_dim_set);

	section = strdup("");
	for (i = 0; i < n; ++i) {
		long lo, len, whole_lo, whole_len;
		char buf[64];

		if (!constant_range(accessed, i, &lo, &len)) {
			free(section);
			section = NULL;
			break;
		}
		if (i > 0 &&
		    (!constant_range(array->extent, i, &whole_lo, &whole_len) ||
		     lo != whole_lo || len != whole_len)) {
			free(section);
			section = NULL;
			break;
		}
		snprintf(buf, sizeof(buf), "[%ld:%ld]", lo, len);
		elements *= len;
		section = append(section, strdup(buf));
	}
	isl_set_free(accessed);

	if (section &&
	    elements * array->element_size > PPCG_REDUCTION_MAX_BYTES) {
		free(section);
		section = NULL;
	}

	return section;
}

char *ppcg_reduction_clause_name(struct ppcg_scop *scop,
	struct ppcg_reduction *red, __isl_keep isl_union_set *domain)
{
	const char *name;
	char *section;

	name = ppcg_reduction_name(red);
	if (!name)
		return NULL;
	if (ppcg_reduction_is_scalar(red))
		return strdup(name);

	section = reduction_section(scop, red, domain);
	if (!section)
		return NULL;

	return append(strdup(name), section);
}

/* The iterations of the statement that performs "red".
 */
static __isl_give isl_union_set *reduction_domain(struct ppcg_reduction *red)
{
	return isl_union_set_from_set(isl_map_domain(reduction_access(red)));
}

/* The pairs of iterations of "red" that accumulate into the same
 * location.
 *
 * Two iterations that accumulate into different locations do not
 * interfere, and the dependences between them, of which there are none,
 * are not what is being relaxed here.  When the accumulator is a scalar
 * the location is the same for every iteration and this is every pair.
 */
__isl_give isl_union_map *ppcg_reduction_same_location(
	struct ppcg_reduction *red)
{
	isl_map *acc, *rev, *same;

	acc = reduction_access(red);
	rev = isl_map_reverse(isl_map_copy(acc));
	same = isl_map_apply_range(acc, rev);

	return isl_union_map_from_map(same);
}

/* Every ordering between the iterations of one accumulation.
 *
 * What may be relaxed is not merely the dependence from one iteration to
 * the next, but every constraint that says one of them has to run before
 * another, because the result is the same whichever order they run in.
 * With live range reordering in particular, ppcg states that ordering
 * transitively, so removing only the immediate dependences would leave
 * the rest of it in place and the loop sequential.
 *
 * The orderings are read off the original schedule: two iterations of the
 * accumulation are ordered exactly when one is scheduled before the other.
 *
 * Only the iterations of the one statement are considered.  Taking every
 * iteration that touches any accumulator at once would also drop the
 * orderings between different accumulations, and those are ordinary
 * dependences: a statement that accumulates into one location while
 * reading another accumulator has to run after the accumulation into
 * that other location has finished.
 *
 * Nothing is dropped for an accumulation whose location is only known
 * once the code runs, as in h[idx[i]] += a[i].  The scop describes such
 * a statement over a domain that also carries the value read from idx,
 * which the schedule does not, so the intersection below leaves nothing
 * and the loop stays sequential.  That is the safe answer: the pairs of
 * iterations that touch the same location cannot be told apart from the
 * ones that do not, and the statement may well carry a dependence that
 * has nothing to do with accumulating.
 */
static __isl_give isl_union_map *reduction_orderings(struct ppcg_scop *scop,
	struct ppcg_reduction *red)
{
	isl_union_set *dom;
	isl_union_map *sched, *orderings;

	dom = reduction_domain(red);
	sched = isl_schedule_get_map(scop->schedule);
	sched = isl_union_map_intersect_domain(sched, dom);

	orderings = isl_union_map_lex_lt_union_map(isl_union_map_copy(sched),
							sched);

	return isl_union_map_intersect(orderings,
					ppcg_reduction_same_location(red));
}

__isl_give isl_union_map *ppcg_reduction_dependences(struct ppcg_scop *scop,
	struct ppcg_reductions *reductions)
{
	isl_union_map *deps = NULL;
	int i;

	if (!scop || !reductions)
		return NULL;

	for (i = 0; i < reductions->n; ++i) {
		struct ppcg_reduction *red = &reductions->red[i];
		isl_union_map *orderings;

		orderings = reduction_orderings(scop, red);
		if (!deps)
			deps = orderings;
		else
			deps = isl_union_map_union(deps, orderings);
	}

	return deps;
}
