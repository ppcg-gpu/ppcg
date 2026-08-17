#define N 1024

/* Accumulations of a value worked out from the iteration itself.
 *
 * The scop writes such a value as an access to no array at all, which
 * is a thing to be asked about carefully: asking which array it belongs
 * to is an error rather than a question with an empty answer.  It is an
 * integer, so an integer accumulator may take it.
 */
void iterator(float a[N], float *pw, int *pc)
{
	float weighted = 0.0f;
	int counted = 0;
#pragma scop
	for (int i = 0; i < N; ++i)
		weighted += a[i] * i;
	for (int i = 0; i < N; ++i)
		counted += i * 2;
#pragma endscop
	*pw = weighted;
	*pc = counted;
}
