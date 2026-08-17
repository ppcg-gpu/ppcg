#define N 1024

/* Two accumulations, where the second reads the accumulator of the
 * first.
 *
 * The iterations of each accumulation may be run in any order, but the
 * two accumulations may not be interleaved: the second loop needs the
 * finished total.  The loops also have the same trip count, so a
 * schedule that ignored that would happily fuse them.
 *
 * The second loop must not name "total" in its clause either, since it
 * only reads it, and a thread reading its own copy would start from the
 * identity of the operator instead of from the value it holds.
 *
 * Integers, so that the two results can be compared exactly and no
 * error can hide in a rounding difference.
 */
void chained(int a[N], int b[N], long *ps, long *pt)
{
	long total = 0;
	long sum = 0;
#pragma scop
	for (int i = 0; i < N; ++i)
		total += b[i];
	for (int i = 0; i < N; ++i)
		sum += a[i] * total;
#pragma endscop
	*ps = sum;
	*pt = total;
}
