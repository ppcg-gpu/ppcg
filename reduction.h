#ifndef REDUCTION_H
#define REDUCTION_H

#include <isl/id.h>
#include <isl/union_map.h>

#include <pet.h>

#include "ppcg.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* An accumulation performed by a statement of a scop.
 *
 * A statement of the form
 *
 *	X[f(i)] = X[f(i)] op expr(i)
 *
 * written with a compound assignment, with "op" associative and
 * commutative, computes a value that does not depend on the order in
 * which the iterations are run.  The dependences it carries on X are
 * therefore not an obstacle to running them in parallel, provided the
 * generated code puts the partial results back together.
 *
 * "stmt" is the position of the statement in the scop.
 * "op" is the operator, one of the compound assignments.
 * "ref" identifies the access to the accumulator, so that the
 *	dependences it carries can be told apart from the others.
 * "index" is the location being accumulated into.
 */
struct ppcg_reduction {
	int stmt;
	enum pet_op_type op;
	isl_id *ref;
	isl_multi_pw_aff *index;
};

/* The accumulations found in a scop.
 */
struct ppcg_reductions {
	int n;
	struct ppcg_reduction *red;
};

/* Find the accumulations performed by the statements of "scop".
 */
struct ppcg_reductions *ppcg_find_reductions(struct ppcg_scop *scop);
void ppcg_reductions_free(struct ppcg_reductions *reductions);

/* Print what was found, one accumulation per line, for the tests to
 * check.  The output names no files and no addresses.
 */
void ppcg_reductions_print(FILE *out, struct ppcg_scop *scop,
	struct ppcg_reductions *reductions);

#if defined(__cplusplus)
}
#endif

#endif
