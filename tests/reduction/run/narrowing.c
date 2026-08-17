#define N 1000

struct member { double x; };
enum floaty { ZERO };

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
 * so none of these loops may be marked parallel.
 *
 * Which type a name stands for is only known from the name, so the
 * types are recognised by their spelling.  The last three are the ones
 * that spelling could get wrong: a floating point type whose name says
 * nothing of the sort, a member of a structure, which the scop names
 * after the structure rather than after the member, and a type whose
 * name has "float" in it and is an enumeration.
 */
void narrowing(int a[N], double d[N], _Float16 f[N], struct member m[N],
	_Bool *pb, int *pi, int *ph, int *pm, enum floaty *pe)
{
	_Bool flag = 0;
	int truncated = 0;
	int halved = 0;
	int membered = 0;
	enum floaty enumerated = ZERO;
#pragma scop
	for (int i = 0; i < N; ++i)
		flag ^= a[i];
	for (int i = 0; i < N; ++i)
		truncated += d[i];
	for (int i = 0; i < N; ++i)
		halved += f[i];
	for (int i = 0; i < N; ++i)
		membered += m[i].x;
	for (int i = 0; i < N; ++i)
		enumerated += d[i];
#pragma endscop
	*pb = flag;
	*pi = truncated;
	*ph = halved;
	*pm = membered;
	*pe = enumerated;
}
