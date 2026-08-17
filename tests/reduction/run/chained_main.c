#include <stdio.h>

#define N 1024

void chained(float a[N], float b[N], float *ps, float *pp);

/* The factors stay near one so that the product of a thousand of them
 * remains a number that can be printed and compared; the terms are
 * spread out so that a lost or a too early addition shows up.
 *
 * The terms are also kept small, because the sum is multiplied by the
 * product: the last digits the product loses to being accumulated in a
 * different order are worth that much more in the sum.
 */
int main(void)
{
	static float a[N], b[N];
	float sum, prod;
	int i;

	for (i = 0; i < N; ++i) {
		a[i] = (float) ((i * 37) % 101) / 70.0f;
		b[i] = 1.0f + (float) ((i * 13) % 7 - 3) / 1000.0f;
	}

	chained(a, b, &sum, &prod);
	printf("%.6f %.6f\n", sum, prod);

	return 0;
}
