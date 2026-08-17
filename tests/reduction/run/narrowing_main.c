#include <stdio.h>

#define N 1000

struct member { double x; };
enum floaty { ZERO };

void narrowing(int a[N], double d[N], _Float16 f[N], struct member m[N],
	_Bool *pb, int *pi, int *ph, int *pm, enum floaty *pe);

/* Every term makes the boolean true, so accumulating them in any order
 * but from the start leaves it false, and the fractions are negative
 * often enough that where the loop is divided decides how many of them
 * are thrown away.
 */
int main(void)
{
	static int a[N];
	static double d[N];
	static _Float16 f[N];
	static struct member m[N];
	_Bool flag;
	enum floaty enumerated;
	int truncated, halved, membered;
	int i;

	for (i = 0; i < N; ++i) {
		a[i] = 3;
		d[i] = (i % 2) ? -0.7 : 3.0;
		f[i] = (i % 2) ? -0.7f : 3.0f;
		m[i].x = d[i];
	}

	narrowing(a, d, f, m, &flag, &truncated, &halved, &membered,
		&enumerated);
	printf("%d %d %d %d %d\n", (int) flag, truncated, halved, membered,
		(int) enumerated);

	return 0;
}
