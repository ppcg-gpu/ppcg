#include <stdio.h>

#define N 1000

struct value { double v; };
struct count { int v; };
struct holder { struct count b[4]; };

extern struct value a;
extern int a_v;
extern struct holder q;
extern struct value q_b[4];
extern int reached;
extern int total;

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
	for (i = 0; i < 4; ++i)
		q_b[i].v = (i % 2) ? -0.7 : 3.0;
	q.b[0].v = 1;
	a_v = 0;
	total = 0;

	collision(b);
	printf("%d %g %d %d\n", a_v, a.v, reached, total);

	return 0;
}
