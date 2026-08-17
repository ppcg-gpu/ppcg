#include <stdio.h>

#define N 1000

struct pair { double v; };

extern struct pair a;
extern int a_v;

void collision(double b[N]);

/* Half of the terms have a fraction to lose, so throwing them away at a
 * different point changes the answer.
 */
int main(void)
{
	static double b[N];
	int i;

	for (i = 0; i < N; ++i)
		b[i] = (i % 2) ? -0.7 : 3.0;
	a_v = 0;

	collision(b);
	printf("%d %g\n", a_v, a.v);

	return 0;
}
