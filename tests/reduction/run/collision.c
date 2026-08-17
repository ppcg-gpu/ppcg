#define N 1000

struct pair { double v; };

struct pair a;
int a_v;

/* An accumulator whose name pet has already given to something else.
 *
 * A member of a structure gets an array of its own, named after the
 * structure and the member together, so a.v is an array called a_v.  A
 * program may have a variable of that name as well, and then the two
 * are only told apart by the shape of what they name: the one made for
 * the member reaches the member through the structure, while the
 * variable reaches itself.
 *
 * Told apart by name alone, this accumulation is taken for one into a
 * double.  An int accumulated from an array of double is not an
 * accumulation, since the fraction is thrown away at every step, so the
 * loop has to stay sequential.
 */
void collision(double b[N])
{
#pragma scop
	a.v = 1.0;
	for (int i = 0; i < N; ++i)
		a_v += b[i];
#pragma endscop
}
