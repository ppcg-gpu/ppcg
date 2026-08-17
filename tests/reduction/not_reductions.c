#define N 256
/* None of these may be taken for an accumulation: a plain assignment, two
 * operators whose order matters, a recurrence that reads a different
 * element than the one it writes, and one written under a condition that
 * reads the accumulator, where whether an iteration accumulates depends
 * on how much has been accumulated so far.
 */
void not_reductions(float a[N], float b[N], float s[1])
{
	float capped = 0.0f;
#pragma scop
	for (int i = 0; i < N; ++i)
		b[i] = a[i] * 2.0f;
	for (int i = 0; i < N; ++i)
		s[0] -= a[i];
	for (int i = 1; i < N; ++i)
		a[i] = a[i - 1] + 1.0f;
	for (int i = 0; i < N; ++i)
		b[i] /= a[i];
	for (int i = 0; i < N; ++i)
		if (capped < 100.0f)
			capped += a[i];
#pragma endscop
	b[0] = capped;
}
