/* Feeds a kernel that stores its result through a pointer the caller
 * owns.
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
	float a[64], o[1];

	for (int i = 0; i < 64; ++i)
		a[i] = i + 1.0f;

	o[0] = -1.0f;
	total_ptr(a, o, 64);
	printf("%g\n", o[0]);

	return 0;
}
