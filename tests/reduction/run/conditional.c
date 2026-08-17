#define N 1024

/* An accumulation written under a condition of its own.
 *
 * The iterations that do accumulate may still be run in any order, and
 * the ones that do not are no obstacle to that, so the loop is parallel.
 * pet keeps the if and what it guards together in a single statement,
 * which is why this is worth a test of its own.
 */
void positive(float a[N], float *o)
{
	float sum = 0.0f;
#pragma scop
	for (int i = 0; i < N; ++i)
		if (a[i] > 0.0f)
			sum += a[i];
#pragma endscop
	*o = sum;
}
