/* An accumulation written out rather than compounded.
 *
 * "sum = sum + a[i]" and "sum += a[i]" are one statement written two
 * ways.  Only the compound spelling used to be recognised, so a body
 * that spelled its accumulation out kept a dependence it does not have,
 * lost the parallelism of the loop that held it, and said nothing about
 * either.  The engine bodies this serves are written both ways.
 *
 * Two of the three below are here to hold the boundary rather than to
 * pass.  "prod" commutes the operands, which is the same statement and
 * has to be found.  "diff" subtracts, which does not reassociate, and
 * must not be.
 */
#define N 1024

void expanded(float a[N], float *out)
{
	float sum = 0.0f;
	float prod = 1.0f;
	float diff = 0.0f;
#pragma scop
	for (int i = 0; i < N; ++i)
		sum = sum + a[i];
	for (int i = 0; i < N; ++i)
		prod = a[i] * prod;
	for (int i = 0; i < N; ++i)
		diff = diff - a[i];
#pragma endscop
	out[0] = sum;
	out[1] = prod;
	out[2] = diff;
}
