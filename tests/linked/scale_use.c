/* The caller, in a unit of its own.
 *
 * scale_by is handed 2.0f while this function has a k of its own, so
 * what the loop computes is a[i] * 2 and never a[i] * k.  A body bound
 * to the wrong declaration's parameters computes the second, and the
 * driver prints a different number.
 */
#include "linked.h"

void apply(float *a, float *o, int n, float k)
{
	for (int i = 0; i < n; ++i)
		o[i] = scale_by(a[i], 2.0f) + k;
}
