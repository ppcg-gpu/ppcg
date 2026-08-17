#include <stdio.h>

#define N 256

void operators(int a[N], int *pp, int *pa, int *po, int *px);

/* Each operator has an identity of its own, and a thread that started
 * from the wrong one, or from the value the variable held rather than
 * from the identity, gives an answer that is plainly different.
 */
int main(void)
{
	static int a[N];
	int prod, all, any, odd;
	int i;

	for (i = 0; i < N; ++i)
		a[i] = 1 + (i * 37) % 101;

	operators(a, &prod, &all, &any, &odd);
	printf("%d %d %d %d\n", prod, all, any, odd);

	return 0;
}
