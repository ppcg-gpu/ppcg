#define N 1024
#define B 16
/* Accumulating into a computed but affine location. */
void histogram(int h[B])
{
#pragma scop
	for (int i = 0; i < N; ++i)
		h[i % B] += 1;
#pragma endscop
}
