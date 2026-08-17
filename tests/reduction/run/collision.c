#define N 1000

struct value { double v; };
struct count { int v; };
struct holder { struct count b[4]; };

/* Accumulators whose name pet has already given to something else.
 *
 * A member of a structure gets an array of its own, named after the
 * structure and the member together, and a member of a member is named
 * the same way, so all of
 *
 *	a.v		gives an array called a_v
 *	q.b[i].v	gives an array called q_b_v
 *	q_b[i].v	gives an array called q_b_v as well
 *
 * can collide with a variable, or with each other.  What tells them
 * apart is how each reaches what it names: a_v the variable reaches
 * itself, a.v reaches a member through a structure, q.b[i].v reaches a
 * member through a structure that is itself a member, and q_b[i].v
 * reaches a member through a structure that is not.
 *
 * Told apart by name alone, these accumulations are taken for ones into
 * a double.  An int accumulated from doubles is not an accumulation,
 * since the fraction is thrown away at every step, so none of these
 * loops may be marked parallel.
 */
struct value a;
int a_v;

struct holder q;
struct value q_b[4];
int reached;
int total;

void collision(double b[N])
{
#pragma scop
	a.v = 1.0;
	for (int i = 0; i < N; ++i)
		a_v += b[i];
	reached = q.b[0].v;
	for (int i = 0; i < 4; ++i)
		total += q_b[i].v;
#pragma endscop
}
