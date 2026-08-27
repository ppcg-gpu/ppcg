#include <stdint.h>

/* THE SAME UNION WITH ITS MEMBERS DECLARED THE OTHER WAY ROUND.
 *
 * union_scaled_elements.c puts the two-byte member first, so the member
 * that is FIRST and the member with the SMALLEST element are the same
 * one and the choice between them cannot be seen.  Here float comes
 * first and uint16_t second, and the two rules part company.
 *
 * The canonical member has to be the one with the smallest element.
 * With it, every other element is a whole number of canonical ones and
 * an access through a larger member is the union of "scale" ordinary
 * accesses -- one construction, always affine.  With the first member
 * standing for the storage, an access through a SMALLER element lies
 * inside a canonical element at a floored index, which is a second and
 * quasi-affine construction; pet refuses it rather than answer wrongly,
 * and the anti-dependence this file is about is not built at all.
 *
 * Measured, taking the first member instead of the smallest:
 *
 *     warning: a union whose members differ in size is not supported:
 *     the members share storage and the index would have to be scaled
 *     by the element size
 *
 * and no dep_false line.  With the smallest, the read of g->h[2048] and
 * the write of g->f[1024] are recognised as the same two bytes.
 *
 * llama-dspark's arena-probe.py runs the shape both ways round for this
 * reason; this is the way round the pet suite did not have.
 */
union grp3 { float f[8192]; uint16_t h[16384]; };
void f(union grp3 * g, const float * s, int32_t * out)
{
#pragma scop
	for (int i = 0; i < 4096; i++) g->f[4096 + i] = s[i];
	out[0] = g->h[2048];
	for (int i = 0; i < 4096; i++)
		g->f[i] = g->f[4096 + i] + 1.0f;
#pragma endscop
}
