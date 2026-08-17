#define N 1024
#define B 16

/* An accumulation into an array that the same loop also reaches by
 * another access.
 *
 * A reduction clause names a section of the array, and within the loop
 * the array stands for the copy each thread was given, so an access
 * that is not part of the accumulation would silently be redirected to
 * that copy.  The loop therefore has to stay sequential.
 */
void aliased(float h[2 * B])
{
#pragma scop
	for (int i = 0; i < N; ++i)
		h[i % B] += h[B + (i % B)];
#pragma endscop
}
