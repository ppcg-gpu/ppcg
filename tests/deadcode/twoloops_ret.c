/* Two loops accumulating into one scalar, result returned.
 *
 * Two loops over the same accumulator are what put the dependences into
 * the shape that made the elimination loop spin: the second loop reads
 * what the first one leaves behind, so the backward walk has a chain to
 * follow rather than a single hop.
 */
#include "deadcode.h"

float total_ret(float *a, int n)
{
	float s = 0;

	for (int i = 0; i < n; ++i)
		s += a[i];
	for (int i = 0; i < n; ++i)
		s += 2 * a[i];

	return s;
}
