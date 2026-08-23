/* Two loops accumulating into one scalar, result stored through a
 * pointer.  This is the program the elimination used to spin on.
 *
 * The last write to o is the only root, and reaching the first loop from
 * it means following the chain backwards through the second loop.  The
 * previous formulation widened the live set to its affine hull on every
 * round and cut the result back with the constraints that survived,
 * which is not a monotone step: the live set settled into a state that
 * reproduced itself byte for byte while still failing the subset test
 * that ended the loop, so ppcg never terminated on this file.
 */
#include "deadcode.h"

void total_ptr(float *a, float *o, int n)
{
	float s = 0;

	for (int i = 0; i < n; ++i)
		s += a[i];
	for (int i = 0; i < n; ++i)
		s += 2 * a[i];

	o[0] = s;
}
