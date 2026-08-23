/* One loop, result handed back through the return value.
 *
 * The cell of the dead code matrix where a scop's only output is what it
 * returns: nothing it writes outlives it, so unless the return counts as
 * a use, the accumulation and the loop that builds it are dead.
 */
#include "deadcode.h"

float total_ret(float *a, int n)
{
	float s = 0;

	for (int i = 0; i < n; ++i)
		s += a[i];

	return s;
}
