#define N 256
/* The initialisation is not an accumulation, only the inner statement is. */
void matvec(float A[N][N], float x[N], float y[N])
{
#pragma scop
	for (int i = 0; i < N; ++i) {
		y[i] = 0.0f;
		for (int j = 0; j < N; ++j)
			y[i] += A[i][j] * x[j];
	}
#pragma endscop
}
