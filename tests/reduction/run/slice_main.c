#include <stdio.h>

#define N 200

void slice(float a[N][N], float h[4][4]);

int main(void)
{
	static float a[N][N], h[4][4];
	int i, j;

	for (i = 0; i < N; ++i)
		for (j = 0; j < N; ++j)
			a[i][j] = 1.0f + ((i * 7 + j) % 5) / 8.0f;

	slice(a, h);

	for (i = 0; i < 4; ++i)
		for (j = 0; j < 4; ++j)
			printf("%.4f\n", h[i][j]);

	return 0;
}
