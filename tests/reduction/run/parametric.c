/* An accumulation whose loop runs a number of times only known at run
 * time.
 *
 * Which elements are accumulated into then depends on that number: with
 * n below sixteen only the first n of them are.  The range that can be
 * worked out without knowing n covers all sixteen, and a clause naming
 * those would have every thread's copy added back into elements the
 * program never meant to have, past the end of an h of n elements.  So h
 * may not be named and the loop stays sequential.
 */
void parametric(double *a, double *h, int n)
{
#pragma scop
	for (int i = 0; i < n; ++i)
		h[i % 16] += a[i];
#pragma endscop
}
