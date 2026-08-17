#define N 256

/* Accumulations whose accumulator is a pointer.
 *
 * Adding up an address is as free of an order as adding up a number,
 * so these are accumulations, but an address is not a thing a reduction
 * clause can be asked to put back together, and naming one is rejected
 * by the compiler that has to build the generated code.  So neither loop
 * may be marked parallel.
 */
void pointers(int step[N], int c[N], float *a, float **end, int *g[8])
{
	float *walk = a;
#pragma scop
	for (int i = 0; i < N; ++i)
		walk += step[i];
	for (int i = 0; i < N; ++i)
		g[i % 8] += c[i];
#pragma endscop
	*end = walk;
}
