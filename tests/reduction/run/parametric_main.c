#include <stdio.h>
#include <stdlib.h>

void parametric(double *a, double *h, int n);

/* The arrays are exactly as long as the loop needs, so a copy added back
 * into more elements than that runs off the end of the allocation.
 */
int main(void)
{
	int n = 5;
	double *a = malloc(n * sizeof(double));
	double *h = calloc(n, sizeof(double));
	double total = 0.0;
	int i;

	for (i = 0; i < n; ++i)
		a[i] = i + 1;

	parametric(a, h, n);

	for (i = 0; i < n; ++i)
		total += h[i];
	printf("%.1f\n", total);

	free(a);
	free(h);

	return 0;
}
