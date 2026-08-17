#include <stdio.h>

#define N 4096
#define B 16

void histogram(int a[N], int h[B]);

/* The counts are integers, so a lost update cannot hide in a rounding
 * difference the way it could in a sum of floats: the output is either
 * exactly right or plainly wrong.
 */
int main(void)
{
	static int a[N];
	int h[B];
	int i;

	for (i = 0; i < N; ++i)
		a[i] = 1 + (i * 37) % 101;
	for (i = 0; i < B; ++i)
		h[i] = 0;

	histogram(a, h);

	for (i = 0; i < B; ++i)
		printf("%d\n", h[i]);

	return 0;
}
