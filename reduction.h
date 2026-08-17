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
 * every iteration?
 */
int ppcg_reduction_is_scalar(struct ppcg_reduction *red);

/* The name of the location accumulated into by "red". */
const char *ppcg_reduction_name(struct ppcg_reduction *red);

/* The operator of "red", as it is written in an OpenMP clause. */
const char *ppcg_reduction_op_str(struct ppcg_reduction *red);

/* Every thread is given a copy of everything the clause of its loop
 * names, and the copies are placed on that thread's stack, so a loop
 * that names as much as a stack holds crashes the generated program
 * rather than speeding it up.  A megabyte is an order of magnitude
 * below the eight the default thread stack has, and a loop allowed that
 * much is still left several times faster than running it sequentially.
 *
 * The whole of a loop is weighed against this, not one accumulator of
 * it: eight accumulators of a megabyte apiece are eight megabytes on
 * the stack of every thread, which is a stack.
 */
#define PPCG_REDUCTION_MAX_BYTES	(1024 * 1024)

/* How the accumulator of "red" is written in the reduction clause of a
 * loop that runs the iterations in "domain", or NULL when it cannot be
 * written in one at all, in which case that loop may not be run in
 * parallel.  The caller frees the result, and is told through "bytes"
 * how much of a thread's stack naming it costs.
 *
 * A single location is named on its own.  An element of an array is
 * named through a section covering every element the loop accumulates
 * into, since which of them an iteration accumulates into is only known
 * once the loop runs.
 */
char *ppcg_reduction_clause_name(struct ppcg_scop *scop,
	struct ppcg_reduction *red, __isl_keep isl_union_set *domain,
	long *bytes);

/* The pairs of iterations of "red" that accumulate into the same
 * location.  A loop only has to name the accumulator in a clause if it
 * runs some of these pairs in different threads; when every pair stays
 * within one iteration of the loop, as it does for y[i] += ..., the
 * threads do not share the accumulator to begin with.
 */
__isl_give isl_union_map *ppcg_reduction_same_location(
	struct ppcg_reduction *red);

/* The dependences that only order the iterations of an accumulation with
 * respect to each other.
 *
 * They are real: each iteration reads what the previous one wrote.  What
 * makes them special is that the result does not depend on the order in
 * which they are applied, so a loop carrying nothing else may be run in
 * parallel, provided the generated code keeps the threads from losing
 * each other's updates.
 *
 * They may only be taken out of the test that asks whether a loop of the
 * generated code can be run in parallel.  The same dependences also say
 * that the value written by one iteration is still wanted by another,
 * and dropping them from what constrains the schedule would make every
 * write but the last one dead.
 */
__isl_give isl_union_map *ppcg_reduction_dependences(struct ppcg_scop *scop,
	struct ppcg_reductions *reductions);

#if defined(__cplusplus)
}
#endif

#endif
