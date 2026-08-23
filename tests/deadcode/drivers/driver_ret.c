/* Feeds a kernel that hands its result back through the return value.
 *
 * This file is never translated.  A printing call is left out of a scop
 * on purpose -- see pet's is_printing_call -- so a driver that ppcg had
 * been run over would print nothing whatever the kernel computed, and
 * the comparison would pass for the wrong reason on every cell at once.
 */
#include <stdio.h>

#include "deadcode.h"

int main()
{
	float a[64];

	for (int i = 0; i < 64; ++i)
		a[i] = i + 1.0f;

	printf("%g\n", total_ret(a, 64));

	return 0;
}
