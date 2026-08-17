#include <stdio.h>

#define SMALL 8192
#define LARGE 131072
#define TRIPS (2 * LARGE)

void budget(double a[TRIPS],
	double s0[SMALL], double s1[SMALL], double s2[SMALL],
	double s3[SMALL], double s4[SMALL], double s5[SMALL],
	double s6[SMALL], double s7[SMALL],
	double l0[LARGE], double l1[LARGE], double l2[LARGE],
	double l3[LARGE], double l4[LARGE], double l5[LARGE],
	double l6[LARGE], double l7[LARGE], double t[TRIPS]);

static double a[TRIPS], t[TRIPS];
static double s[8][SMALL];
static double l[8][LARGE];

/* The counts are whole numbers a double holds exactly, so the two
 * results are compared exactly.  A loop that ran out of stack prints
 * nothing at all, which is a difference as well.
 */
int main(void)
{
	double sum_s = 0.0, sum_l = 0.0;
	int i, j;

	for (i = 0; i < TRIPS; ++i)
		a[i] = i % 7;

	budget(a, s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7],
		l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], t);

	for (j = 0; j < 8; ++j) {
		for (i = 0; i < SMALL; ++i)
			sum_s += s[j][i];
		for (i = 0; i < LARGE; ++i)
			sum_l += l[j][i];
	}
	printf("%.1f %.1f\n", sum_s, sum_l);

	return 0;
}
