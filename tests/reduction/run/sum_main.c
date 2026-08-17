#include <stdio.h>

#define N 1024

float total(float a[N]);

/* The values are spread out enough that a wrong order of accumulation
 * would show up in the printed result, and none of them is zero, so an
 * iteration that is dropped or run twice changes the sum whichever one
 * it is.  The smallest term is a seventh, which is more than the
 * comparison tolerates.
 */
int main(void)
{
	static float a[N];
	int i;

	for (i = 0; i < N; ++i)
		a[i] = (float) (1 + (i * 37) % 101) / 7.0f;

	printf("%.6f\n", total(a));

	return 0;
}
