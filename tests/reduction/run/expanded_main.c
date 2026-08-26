#include <stdio.h>

#define N 1024

float total(float a[N]);

/* The same data as the compound spelling's run test, so the two answers
 * can be compared to each other as well as each to its own reference.
 * The values are spread out enough that a wrong order of accumulation
 * would show in the printed result, and none of them is zero, so an
 * iteration dropped or run twice changes the sum whichever it is.
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
