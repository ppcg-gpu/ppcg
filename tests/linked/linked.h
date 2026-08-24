/* What the linked corpus hands to its driver.
 *
 * scale_by is defined in one unit and called from another, which is the
 * only reason this corpus has two of them: a call that crosses a unit is
 * what ppcg_linked exists to put a body in place of, and what an
 * ordinary ppcg run over either file alone cannot see.
 */
#ifndef PPCG_TESTS_LINKED_H
#define PPCG_TESTS_LINKED_H

float scale_by(float x, float k);
void apply(float *a, float *o, int n, float k);

#endif
