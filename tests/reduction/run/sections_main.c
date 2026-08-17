#include <stdio.h>

#define SMALL 4096
#define BIG 262144

void sections(double o[32], double t[8][8], double s[SMALL], double b[BIG]);

/* Each element is counted, so a lost update leaves a count that is
 * plainly wrong, and the counts are whole numbers a double holds
 * exactly.
 */
int main(void)
{
	static double o[32], t[8][8], s[SMALL], b[BIG];
	double sum_s = 0.0, sum_b = 0.0;
	int i, j;

	sections(o, t, s, b);

	for (i = 0; i < 32; ++i)
		printf("%.1f\n", o[i]);
	for (i = 0; i < 8; ++i)
		for (j = 0; j < 8; ++j)
			printf("%.1f\n", t[i][j]);
	for (i = 0; i < SMALL; ++i)
		sum_s += s[i];
	for (i = 0; i < BIG; ++i)
		sum_b += b[i];
	printf("%.1f %.1f %.1f %.1f\n", sum_s, sum_b, s[0], b[0]);

	return 0;
}
