/* Feeds the linked corpus and prints what it computed.
 *
 * Never translated: pet leaves a call that only prints out of the scop
 * on purpose, so a driver ppcg had been run over would print nothing
 * whatever the kernel computed, and the comparison would pass for that
 * reason rather than for the right one.
 */
#include <stdio.h>

#include "linked.h"

int main()
{
	float a[16], o[16];

	for (int i = 0; i < 16; ++i)
		a[i] = i + 1.0f;

	apply(a, o, 16, 100.0f);

	for (int i = 0; i < 16; ++i)
		printf("%g\n", o[i]);

	return 0;
}
