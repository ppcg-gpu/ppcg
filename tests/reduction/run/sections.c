#define SMALL 65536
#define BIG 262144

/* The shapes an array section can and cannot take.
 *
 * "o" is accumulated into from the middle of the array, so the section
 * has to start where the accumulation does rather than at zero.  "t" is
 * two dimensional, so the section needs a range per dimension.  "s" is
 * half a megabyte, which each thread can be given a copy of, while "b"
 * is two megabytes, which would not fit on a thread's stack, so that
 * loop has to stay sequential.
 */
void sections(double o[32], double t[8][8], double s[SMALL], double b[BIG])
{
#pragma scop
	for (int i = 0; i < 1024; ++i)
		o[16 + (i % 16)] += 1.0;
	for (int i = 0; i < 1024; ++i)
		t[i % 8][(i / 8) % 8] += 1.0;
	for (int i = 0; i < 2 * SMALL; ++i)
		s[i % SMALL] += 1.0;
	for (int i = 0; i < BIG + 1; ++i)
		b[i % BIG] += 1.0;
#pragma endscop
}
