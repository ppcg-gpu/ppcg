#define N 256
/* None of these may be taken for an accumulation: a plain assignment, two
 * operators whose order matters, and a recurrence that reads a different
 * element than the one it writes.
 */
void not_reductions(float a[N], float b[N], float s[1])
{
#pragma scop
	for (int i = 0; i < N; ++i)
		b[i] = a[i] * 2.0f;
	for (int i = 0; i < N; ++i)
		s[0] -= a[i];
	for (int i = 1; i < N; ++i)
		a[i] = a[i - 1] + 1.0f;
	for (int i = 0; i < N; ++i)
		b[i] /= a[i];
#pragma endscop
}
