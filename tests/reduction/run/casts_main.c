#include <stdio.h>

#define N 1000

void casts(double d[N], int a[N], int b[N], int *pk, int *pt);

/* Half of the terms have a fraction to lose and the other half do not,
 * so throwing them away at a different point changes the answer.
 */
int main(void)
{
	static double d[N];
	static int a[N], b[N];
	int kept, thrown;
	int i;

	for (i = 0; i < N; ++i) {
		d[i] = (i % 2) ? -0.7 : 3.0;
		a[i] = (i % 2) ? -7 : 3;
		b[i] = (i % 2) ? 10 : 1;
	}

	casts(d, a, b, &kept, &thrown);
	printf("%d %d\n", kept, thrown);

	return 0;
}
