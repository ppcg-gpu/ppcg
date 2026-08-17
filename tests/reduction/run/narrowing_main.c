#include <stdio.h>

#define N 1000

void narrowing(int a[N], double d[N], _Bool *pb, int *pi);

/* Every term makes the boolean true, so accumulating them in any order
 * but from the start leaves it false, and the fractions are negative
 * often enough that where the loop is divided decides how many of them
 * are thrown away.
 */
int main(void)
{
	static int a[N];
	static double d[N];
	_Bool flag;
	int truncated;
	int i;

	for (i = 0; i < N; ++i) {
		a[i] = 3;
		d[i] = (i % 2) ? -0.7 : 3.0;
	}

	narrowing(a, d, &flag, &truncated);
	printf("%d %d\n", (int) flag, truncated);

	return 0;
}
