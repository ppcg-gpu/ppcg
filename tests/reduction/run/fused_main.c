#include <stdio.h>

#define N 1000

void fused(int a[N], int c[N], int h[16]);

int main(void)
{
	static int a[N], c[N];
	int h[16];
	int i;

	for (i = 0; i < N; ++i)
		a[i] = 1 + (i * 37) % 101;
	for (i = 0; i < 16; ++i)
		h[i] = 0;

	fused(a, c, h);

	for (i = 0; i < 16; ++i)
		printf("%d\n", h[i]);
	printf("%d %d\n", c[0], c[N - 1]);

	return 0;
}
