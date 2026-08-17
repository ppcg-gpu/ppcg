#include <stdio.h>

#define N 256

void matvec(float A[N][N], float x[N], float y[N]);

int main(void)
{
	static float A[N][N], x[N], y[N];
	int i, j;

	for (i = 0; i < N; ++i) {
		x[i] = (float) (1 + (i * 37) % 101) / 101.0f;
		for (j = 0; j < N; ++j)
			A[i][j] = (float) (1 + (i * 13 + j * 7) % 97) / 97.0f;
	}

	matvec(A, x, y);

	for (i = 0; i < N; ++i)
		printf("%.6f\n", y[i]);

	return 0;
}
