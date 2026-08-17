#include <stdio.h>

#define N 1024

void positive(float a[N], float *o);

/* Roughly half of the terms are negative, so a loop that accumulated
 * the ones it was told to skip would be off by a wide margin.  The
 * smallest positive term is a seventh, which is more than the
 * comparison tolerates, so a dropped iteration shows up as well.
 */
int main(void)
{
	static float a[N];
	float sum;
	int i;

	for (i = 0; i < N; ++i)
		a[i] = (float) ((i * 37) % 101 - 50) / 7.0f;

	positive(a, &sum);
	printf("%.6f\n", sum);

	return 0;
}
