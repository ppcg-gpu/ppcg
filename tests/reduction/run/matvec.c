#define N 256

/* An accumulation into an array element that every thread has to
 * itself.
 *
 * Each iteration of the parallel loop accumulates into a y of its own,
 * so the threads do not share it and it must not be named in a clause.
 * Naming it would have every thread privatise the whole of y for
 * nothing.
 */
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
