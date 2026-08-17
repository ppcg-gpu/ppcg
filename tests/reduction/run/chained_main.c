#include <stdio.h>

#define N 1024

void chained(int a[N], int b[N], long *ps, long *pt);

/* No term is zero, so an iteration that is dropped or run twice changes
 * the result whichever one it is.  That matters most at the ends of the
 * range, where a wrongly divided loop would lose one.
 */
int main(void)
{
	static int a[N], b[N];
	long sum, total;
	int i;

	for (i = 0; i < N; ++i) {
		a[i] = 1 + (i * 37) % 101;
		b[i] = 1 + (i * 13) % 7;
	}

	chained(a, b, &sum, &total);
	printf("%ld %ld\n", sum, total);

	return 0;
}
