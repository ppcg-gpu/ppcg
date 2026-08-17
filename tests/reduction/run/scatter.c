#define N 1024
#define B 16

/* An accumulation whose location is only known once the loop runs.
 *
 * The scop describes this statement over a domain that also carries the
 * value read from idx, which the schedule does not, so the pairs of
 * iterations that accumulate into the same element cannot be told apart
 * from the ones that do not and nothing may be dropped.  The loop has to
 * stay sequential.
 */
void scatter(int idx[N], float a[N], float h[B])
{
#pragma scop
	for (int i = 0; i < N; ++i)
		h[idx[i]] += a[i];
#pragma endscop
}
