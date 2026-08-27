#include <stdlib.h>

/* ONE BUFFER TOUCHED BOTH WAYS: BY NAME AND THROUGH A PARAMETER.
 *
 * A `#pragma ppcg arena` map is attached where an array's NAME is
 * written, and after inlining the body of a helper writes a parameter
 * instead.  So a buffer the source touches both ways -- once directly,
 * once by handing "&m[i]" to a helper that is put in place -- comes out
 * split in two: the direct access composed onto the storage's
 * representative, the inlined one left on the member.  To isl those are
 * two unrelated arrays, no flow edge can join them, and nothing orders
 * the write against the read.
 *
 * The substitution is the moment the body's access learns which array it
 * really touches, so it is where the map has to travel.
 *
 * Measured, with the carry disabled in substitute_access:
 *
 *     -  for (int c0 = 0; c0 <= 1023; c0 += 1) {
 *     -    m[c0] = ((c0) + 1.0f);
 *     -    o[c0] = m[c0];
 *     +  {
 *     +    for (int c0 = 0; c0 <= 1023; c0 += 1)
 *     +      o[c0] = m[c0];
 *     +    for (int c0 = 0; c0 <= 1023; c0 += 1)
 *     +      m[c0] = ((c0) + 1.0f);
 *
 * The read loop is scheduled BEFORE the loop that fills what it reads,
 * so o keeps the -1.0f main put in the storage and this program returns
 * EXIT_FAILURE.  With the carry it returns EXIT_SUCCESS.
 *
 * m is a member of r's storage at byte 4096, which is r[1024], and
 * everything is float so that the aliasing is the standard's and not the
 * optimiser's to disbelieve -- see tests/arena_annotation.c for what a
 * pun costs here.
 */
static float storage[2048];

static void use(const float * p, float * o, int i)
{
	o[i] = p[0];
}

void f(float * r, float * m, float * o)
{
#pragma ppcg arena r 0 m 4096
#pragma scop
	for (int i = 0; i < 1024; i++)
		m[i] = i + 1.0f;
	for (int i = 0; i < 1024; i++)
		use(&m[i], o, i);
#pragma endscop
}

int main()
{
	float *r = &storage[0];
	float *m = &storage[1024];
	static float o[1024];

	for (int i = 0; i < 2048; i++)
		storage[i] = -1.0f;
	for (int i = 0; i < 1024; i++)
		o[i] = 0.0f;

	f(r, m, o);

	for (int i = 0; i < 1024; i++)
		if (o[i] != i + 1.0f)
			return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
