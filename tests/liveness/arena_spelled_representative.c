/* A NAME THE ANNOTATION MENTIONS AND THE SOURCE DOES SUBSCRIPT.
 *
 * arena_add_arrays adds the arrays a `#pragma ppcg arena` names and are
 * not there already, and says which ones it had to invent.  It asked the
 * array set whether it held the id it had just built from the
 * ValueDecl -- but the ids in that set have been through
 * pet_expr_anonymize and carry no decl, the one built there carries one,
 * and isl matches on the OBJECT.  The answer was therefore always no:
 * every representative got a second array beside its real one, and every
 * one of them was reported as never spelled.
 *
 * On the 402-node scop that report named all twelve representatives the
 * pragmas mention, among them names the source subscripts in plain
 * sight.  Twelve names against twelve pragmas was the tell.
 *
 * Here `rep` is written and read by name.  What this cell holds is a
 * SILENCE: ppcg must not claim it was never spelled.  Asking by name
 * rather than by id object is what buys that, and the .says line is
 * negated because what the fix bought is a false sentence no longer
 * being printed -- there is no new sentence to look for.
 */
void f(float * rep, float * mem, float * o)
{
#pragma ppcg arena rep 0 mem 256
#pragma scop
	for (int i = 0; i < 64; i++)
		rep[i] = i;
	for (int i = 0; i < 64; i++)
		mem[i] = rep[i] + 1.0f;
	for (int i = 0; i < 64; i++)
		o[i] = mem[i];
#pragma endscop
}
