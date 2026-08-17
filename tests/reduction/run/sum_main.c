#include <stdio.h>

#define N 1024

float total(float a[N]);

/* The values are spread out enough that a wrong order of accumulation
 * would show up in the printed result.
 */
int main(void)
{
	static float a[N];
	int i;

	for (i = 0; i < N; ++i)
		a[i] = (float) ((i * 37) % 101) / 7.0f;

	printf("%.6f\n", total(a));

	return 0;
}
