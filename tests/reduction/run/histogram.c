#define N 4096
#define B 16

/* An accumulation into an element of an array, where which element it is
 * depends on the iteration.
 *
 * The location cannot be named in a reduction clause, so the iterations
 * that share one are made to take turns instead.  Every bucket is
 * accumulated into by many iterations of the one parallel loop, so the
 * counts only come out right if that really happens.
 */
void histogram(int a[N], int h[B])
{
#pragma scop
	for (int i = 0; i < N; ++i)
		h[i % B] += a[i];
#pragma endscop
}
