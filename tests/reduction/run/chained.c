#define N 1024

/* Two accumulations, where the second reads the accumulator of the first.
 *
 * The iterations of each accumulation may be run in any order, but the
 * two accumulations may not be interleaved: the second loop needs the
 * finished product.  The loops also have the same trip count, so a
 * schedule that ignored that would happily fuse them.
 *
 * The second loop must not name "prod" in its clause either, since it
 * only reads it, and a thread reading its own copy would start from the
 * identity of the operator instead of from the value it holds.
 */
void chained(float a[N], float b[N], float *ps, float *pp)
{
	float prod = 1.0f;
	float sum = 0.0f;
#pragma scop
	for (int i = 0; i < N; ++i)
		prod *= b[i];
	for (int i = 0; i < N; ++i)
		sum += a[i] * prod;
#pragma endscop
	*ps = sum;
	*pp = prod;
}
