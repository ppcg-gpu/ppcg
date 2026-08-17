#define SMALL 8192
#define LARGE 131072
#define TRIPS (2 * LARGE)

/* What a loop asks a thread to hold, all of it at once.
 *
 * Every thread is given a copy of everything the clause names, and the
 * copies go on that thread's stack, so what has to fit is the whole of
 * the clause and not each accumulator of it.  The first loop names
 * eight arrays of sixty-four kilobytes, which fits; the second names
 * eight of a megabyte, which is a stack, and has to stay sequential
 * rather than crash the program it was generated from.
 *
 * Both loops run twice over each array, so that the threads really do
 * share the elements: a loop that touched each of them once would need
 * no clause at all and would say nothing about what a clause may name.
 */
void budget(double a[TRIPS],
	double s0[SMALL], double s1[SMALL], double s2[SMALL],
	double s3[SMALL], double s4[SMALL], double s5[SMALL],
	double s6[SMALL], double s7[SMALL],
	double l0[LARGE], double l1[LARGE], double l2[LARGE],
	double l3[LARGE], double l4[LARGE], double l5[LARGE],
	double l6[LARGE], double l7[LARGE], double t[TRIPS])
{
#pragma scop
	for (int i = 0; i < TRIPS; ++i) {
		t[i] = a[i];
		s0[i % SMALL] += t[i];
		s1[i % SMALL] += t[i];
		s2[i % SMALL] += t[i];
		s3[i % SMALL] += t[i];
		s4[i % SMALL] += t[i];
		s5[i % SMALL] += t[i];
		s6[i % SMALL] += t[i];
		s7[i % SMALL] += t[i];
	}
	for (int i = 0; i < TRIPS; ++i) {
		t[i] = a[i];
		l0[i % LARGE] += t[i];
		l1[i % LARGE] += t[i];
		l2[i % LARGE] += t[i];
		l3[i % LARGE] += t[i];
		l4[i % LARGE] += t[i];
		l5[i % LARGE] += t[i];
		l6[i % LARGE] += t[i];
		l7[i % LARGE] += t[i];
	}
#pragma endscop
}
