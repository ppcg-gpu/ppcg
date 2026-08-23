/* What the dead code corpus hands to its drivers.
 *
 * Each kernel file defines exactly one of these, and is the only thing
 * ppcg is asked to translate.  The driver that calls it is compiled from
 * source both times, so what the test compares is the work the kernel
 * did, not the printing around it.
 */
#ifndef PPCG_TESTS_DEADCODE_H
#define PPCG_TESTS_DEADCODE_H

float total_ret(float *a, int n);
void total_ptr(float *a, float *o, int n);

#endif
