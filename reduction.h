#ifndef REDUCTION_H
#define REDUCTION_H

#include <isl/id.h>
#include <isl/union_map.h>
#include <isl/union_set.h>

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

/* Is the location accumulated into by "red" a single one, the same for
 * every iteration?  Only then can each thread be given a copy of it and
 * the copies combined afterwards, which is what the generated code does.
 */
int ppcg_reduction_is_scalar(struct ppcg_reduction *red);

/* The name of the location accumulated into by "red". */
const char *ppcg_reduction_name(struct ppcg_reduction *red);

/* The operator of "red", as it is written in an OpenMP clause. */
const char *ppcg_reduction_op_str(struct ppcg_reduction *red);

/* The iterations of the statement that performs "red".  A loop only has
 * to name the accumulator in a clause if these are among the iterations
 * it runs.
 */
__isl_give isl_union_set *ppcg_reduction_domain(struct ppcg_reduction *red);

/* The dependences that only order the iterations of an accumulation with
 * respect to each other.
 *
 * They are real: each iteration reads what the previous one wrote.  What
 * makes them special is that the result does not depend on the order in
 * which they are applied, so a schedule is free to ignore them, provided
 * the generated code gives every thread its own copy of the accumulator
 * and combines the copies afterwards.
 *
 * They may only be taken out of what constrains the schedule.  The same
 * dependences also say that the value written by one iteration is still
 * wanted by another, and dropping them there would make every write but
 * the last one dead.
 *
 * Only accumulations into a single location are described, since those
 * are the ones the backend can currently combine.
 */
__isl_give isl_union_map *ppcg_reduction_dependences(struct ppcg_scop *scop,
	struct ppcg_reductions *reductions);

#if defined(__cplusplus)
}
#endif

#endif
