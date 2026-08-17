#define N 1000

/* An accumulation into an array in a loop that runs another statement
 * as well.
 *
 * The section named in the clause covers the elements this loop
 * accumulates into, which means picking the iterations of the one
 * statement out of everything the loop runs rather than taking the
 * whole of it for a set of its own.
 */
void fused(int a[N], int c[N], int h[16])
{
#pragma scop
	for (int i = 0; i < N; ++i) {
		c[i] = a[i] * 2;
		h[i % 16] += c[i];
	}
#pragma endscop
}
