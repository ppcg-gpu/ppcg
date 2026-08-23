/* A whole function body is a scop when scops are autodetected, and this
 * one ends by returning what it computed.  Nothing it writes outlives
 * it: s is declared inside the scop and killed at the end of it, so
 * unless the return counts as a use, the sum and the loop that builds
 * it are dead and the generated code computes nothing at all.
 *
 * This is the ordinary case for a linked AST, which is only ever read
 * with autodetection on, and not a corner of it.
 */
float total(float *a, int n)
{
	float s = 0;

	for (int i = 0; i < n; ++i)
		s += a[i];

	return s;
}
