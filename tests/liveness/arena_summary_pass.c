float * pick(float * p);

/* SCAN_ARRAYS IS CALLED TWICE AND ONLY ONE OF THE TWO IS THE SCOP.
 *
 * The first is a CALLED function's body, built to compute its summary
 * and freed immediately after.  pet_arena_map is file-scope and still
 * holds the caller's pragmas, so adding the annotation's arrays there
 * invents them in a scop that mentions none of them -- and reports every
 * one as never spelled.
 *
 * That report is what sent a whole reading in the wrong direction: on
 * the 402-node scop it named all twelve representatives the pragmas
 * mention, nine of which the source subscribes in plain sight, and was
 * read as "pet does not see accesses that are there".  It sees them.
 * The second pass named three, and those three really are never
 * subscripted.
 *
 * `helper` here is not put in place, because `pick(o)` is not an
 * argument pet can resolve to an access -- pet says so and computes a
 * summary instead, which is the whole point: the summary pass is the
 * one that must not touch the caller's annotation.  With the flag on
 * both calls, this file prints the sentence twice, once for the
 * summary's scop and once for the real one.
 */
void helper(float * q, int n)
{
	for (int i = 0; i < n; i++)
		q[i] = q[i] * 2.0f;
}

void f(float * rep, float * mem, float * o)
{
#pragma ppcg arena rep 0 mem 256
#pragma scop
	for (int i = 0; i < 64; i++)
		mem[i] = i;
	helper(pick(o), 64);
	for (int i = 0; i < 64; i++)
		o[i] = mem[i];
#pragma endscop
}
