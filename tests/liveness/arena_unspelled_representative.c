/* AN ARRAY THE ANNOTATION NAMES AND THE SOURCE NEVER SUBSCRIPTS.
 *
 * `rep` is the storage; `mem` is a member of it, 256 bytes in.  Nothing
 * in the scop writes `rep[...]` -- it is only ever the name in the
 * pragma, which is the ordinary shape when an allocator hands out one
 * buffer and every tensor is a view into it.
 *
 * pet_scop_collect_arrays builds the array list from ACCESSES.
 * Composition replaces a member's access relation and leaves its index
 * naming the member, so a representative nobody subscripts got no array
 * at all -- and scop_collect_accesses, which intersects every access's
 * range with the extents, then removed every access every member had
 * composed onto it.  The annotation became a silent no-op and the
 * aliasing it exists to declare was invisible again.
 *
 * The measured shape of that, on this file with arena_add_arrays taken
 * out of scan_arrays: `reads` comes back EMPTY, `may_writes` keeps only
 * the store to `o`, and dep_flow is empty -- so nothing relates the loop
 * that fills `mem` to the loop that reads it, and a scheduler is free to
 * put them in either order.  With it, both accesses are there under the
 * representative's name at 64 + i, and the flow between them with them.
 *
 * The claim is about relations rather than generated code: an access
 * that was dropped is not misplaced but absent, and the consequence
 * arrives later as a fusion nobody forbade.  Hence a .deps companion.
 */
void f(float * rep, float * mem, float * o)
{
#pragma ppcg arena rep 0 mem 256
#pragma scop
	for (int i = 0; i < 64; i++)
		mem[i] = i;
	for (int i = 0; i < 64; i++)
		o[i] = mem[i];
#pragma endscop
}
