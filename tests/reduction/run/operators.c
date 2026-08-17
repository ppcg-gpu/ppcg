#define N 256

/* The operators other than +, so that each one is recognised and named
 * as itself.  A product of floats would run away, so these accumulate
 * into integers, where every one of them keeps its value in range.
 */
void operators(int a[N], int *pp, int *pa, int *po, int *px)
{
	int prod = 1;
	int all = -1;
	int any = 0;
	int odd = 0;
#pragma scop
	for (int i = 0; i < N; ++i)
		prod *= 1 + (a[i] & 1);
	for (int i = 0; i < N; ++i)
		all &= a[i];
	for (int i = 0; i < N; ++i)
		any |= a[i];
	for (int i = 0; i < N; ++i)
		odd ^= a[i];
#pragma endscop
	*pp = prod;
	*pa = all;
	*po = any;
	*px = odd;
}
