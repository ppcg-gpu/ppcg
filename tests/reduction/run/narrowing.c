#define N 1000

/* Accumulations that are put back into a type that does not keep what
 * the operator did.
 *
 * A compound assignment is x = (T) (x op y), so the value is stored back
 * into T at every step.  Storing an integer in a narrower integer keeps
 * the low bits, which is all these operators work on, but storing
 * anything in a boolean turns everything that is not zero into one, and
 * storing a floating point value in an integer throws away the fraction.
 * A thread that starts from the identity of the operator rather than
 * from what the loop has accumulated so far then arrives somewhere else,
 * so neither loop may be marked parallel.
 */
void narrowing(int a[N], double d[N], _Bool *pb, int *pi)
{
	_Bool flag = 0;
	int truncated = 0;
#pragma scop
	for (int i = 0; i < N; ++i)
		flag ^= a[i];
	for (int i = 0; i < N; ++i)
		truncated += d[i];
#pragma endscop
	*pb = flag;
	*pi = truncated;
}
