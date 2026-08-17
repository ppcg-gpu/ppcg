#define N 1000

/* Two accumulations of a cast value.
 *
 * The first truncates every term before adding it, so what is added up
 * is whole numbers and the order does not matter.  The second works the
 * term out as a floating point value and puts it back into an int, which
 * throws away the fraction at every step, so where the loop is divided
 * decides how many times that happens.
 *
 * They are told apart by the type the cast casts to, and not by what it
 * casts, which is an int in both.
 */
void casts(double d[N], int a[N], int b[N], int *pk, int *pt)
{
	int kept = 0;
	int thrown = 0;
#pragma scop
	for (int i = 0; i < N; ++i)
		kept += (int) d[i];
	for (int i = 0; i < N; ++i)
		thrown += (float) a[i] / b[i];
#pragma endscop
	*pk = kept;
	*pt = thrown;
}
