/* One loop, result handed back through a pointer the caller owns.
 *
 * The write through the pointer is live out on its own, so this cell
 * says what the elimination does when the root is a store rather than a
 * return: the loop feeding that store has to survive with it.
 */
#include "deadcode.h"

void total_ptr(float *a, float *o, int n)
{
	float s = 0;

	for (int i = 0; i < n; ++i)
		s += a[i];

	o[0] = s;
}
