#include <stdio.h>

#define N 256

void pointers(int step[N], int c[N], float *a, float **end, int *g[8]);

int main(void)
{
	static int step[N], c[N];
	static float a[1];
	static int values[8][N];
	int *g[8];
	float *end;
	int i;

	for (i = 0; i < N; ++i) {
		step[i] = (i % 3) - 1;
		c[i] = 1 + (i * 37) % 101;
	}
	for (i = 0; i < 8; ++i)
		g[i] = values[i];

	pointers(step, c, a, &end, g);

	printf("%ld\n", (long) (end - a));
	for (i = 0; i < 8; ++i)
		printf("%ld\n", (long) (g[i] - values[i]));

	return 0;
}
