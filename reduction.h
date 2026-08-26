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
 * with "op" associative and commutative, computes a value that does not
 * depend on the order in which the iterations are run.  The dependences
 * it carries on X are therefore not an obstacle to running them in
 * parallel, provided the generated code puts the partial results back
 * together.
 *
 * BOTH SPELLINGS ARE THE SAME STATEMENT.  C writes that two ways: as a
 * compound assignment "X op= expr", where the accumulator is one access
 * that both reads and writes, and expanded as "X = X op expr", where it
 * is two accesses, a write and a read.  They compute the same value and
 * a compiler emits the same instructions.  Only the compound form used
 * to be recognised here, so a body that spelled its accumulation out
 * kept a dependence it does not have and lost its parallelism, and
 * nothing said so.
 *
 * "stmt" is the position of the statement in the scop.
 * "op" is the operator, named by its compound assignment whichever
 *	spelling the source used.
 * "ref" identifies the access to the accumulator that WRITES it, so
 *	that the dependences it carries can be told apart from the others.
 * "index" is the location being accumulated into.
 * "n_acc" is how many accesses to the accumulator the statement may
 *	hold: one for the compound spelling, two for the expanded one.
 *	More than that reads the accumulator a second time and is not an
 *	accumulation, whichever way it is written.
 */
struct ppcg_reduction {
	int stmt;
	enum pet_op_type op;
	isl_id *ref;
	isl_multi_pw_aff *index;
	int n_acc;
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
 * names, the copies are placed on that thread's stack, and they are
 * added up when the loop ends.  Both of those set a limit.
 *
 * The stack sets the one that matters for whether the program runs at
 * all: a loop naming as much as a stack holds crashes it.
 *
 * Adding the copies up sets a lower one.  That work is the size of what
 * is named times the number of threads, and no trip count amortises it,
 * so a large enough section leaves the loop slower than the sequential
 * code it came from.  Measured over four million iterations on a machine
 * of eighty-eight threads, against the same loop run sequentially:
 *
 *	    8 KiB	 12 times faster
 *	   64 KiB	  4 times faster
 *	  512 KiB	  3 times slower
 *	    1 MiB	  4 times slower
 *
 * Sixty-four kilobytes is where that crossing was found.  It is a
 * judgement about what is worth doing rather than about what is
 * correct: with few threads even a megabyte still pays, but the number
 * of threads is not something the generated code can be compiled
 * against.
 *
 * The whole of a loop is weighed against this, not one accumulator of
 * it: eight accumulators of the limit apiece are eight times the limit
 * on the stack of every thread, and eight times the combining.
 */
#define PPCG_REDUCTION_MAX_BYTES	(64 * 1024)

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
