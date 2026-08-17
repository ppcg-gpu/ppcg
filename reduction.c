#include <stdio.h>
#include <stdlib.h>

#include <isl/aff.h>
#include <isl/id.h>
#include <isl/multi.h>
#include <isl/space.h>

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

/* Is "expr" a compound assignment that accumulates into the location it
 * reads?  If so, describe it in "red".
 *
 * The first argument of a compound assignment is the accumulator, both
 * read and written.  pet represents it as a single access expression that
 * is marked as both, so the check is that this argument is an access, that
 * it is read as well as written, and that the operator is one whose order
 * does not matter.
 */
static int extract_reduction(__isl_keep pet_expr *expr, int stmt,
	struct ppcg_reduction *red)
{
	enum pet_op_type op;
	pet_expr *arg;
	isl_bool is_read, is_write;
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
	pet_expr_free(arg);

	return ok;
}

/* Look for an accumulation in the body of statement "stmt".
 *
 * Only a statement that is a single expression can be one; anything
 * with control flow of its own has been broken up into separate
 * statements by the time the scop is built.
 */
static int find_in_stmt(struct pet_stmt *stmt, int pos,
	struct ppcg_reduction *red)
{
	pet_expr *body;
	int found;

	if (pet_tree_get_type(stmt->body) != pet_tree_expr)
		return 0;

	body = pet_tree_expr_get_expr(stmt->body);
	if (!body)
		return 0;
	found = extract_reduction(body, pos, red);
	pet_expr_free(body);

	return found;
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
	for (i = 0; i < reductions->n; ++i) {
		isl_id_free(reductions->red[i].ref);
		isl_multi_pw_aff_free(reductions->red[i].index);
	}
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
