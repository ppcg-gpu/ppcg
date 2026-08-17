#define N 256

struct point { float x; };

/* None of these may be taken for an accumulation.
 *
 * A plain assignment and two operators whose order matters are not one
 * to begin with, and neither is a recurrence that reads a different
 * element than the one it writes.
 *
 * The rest are written as compound assignments with an operator whose
 * order does not matter, and are still not accumulations, because the
 * statement does more than accumulate:
 *
 * - the condition reads the accumulator, so whether an iteration
 *   accumulates depends on how much has been accumulated so far;
 * - the right hand side reads the accumulator, so each iteration needs
 *   the value the previous one left rather than merely adding to it;
 * - the statement writes something besides the accumulator, and the
 *   iterations that share an accumulator element share that too;
 * - the accumulator is a member of a structure, which pet gives an
 *   array of its own whose name the generated code does not declare.
 */
void not_reductions(float a[N], float b[N], float s[1], int c[N], int d[8],
	int h[8], struct point *p)
{
	float capped = 0.0f;
	float recurrent = 1.0f;
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
	for (int i = 0; i < N; ++i)
		recurrent += recurrent * 0.5f + a[i];
	for (int i = 0; i < N; ++i)
		h[i % 8] += c[i] * d[i % 8]++;
	for (int i = 0; i < N; ++i)
		p->x += a[i];
#pragma endscop
	b[0] = capped + recurrent;
}
