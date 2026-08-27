#include <stdlib.h>

/* SEPARATELY DECLARED ARRAYS THAT ARE ONE PIECE OF STORAGE.
 *
 * A generator that emits C from a graph gets its buffers from an
 * allocator that reuses a dead tensor's bytes for a later one, so two
 * arrays with different names, different declarations and different
 * printed subscripts are one piece of memory.  A `#pragma ppcg arena`
 * says which of them share storage and at what byte offset, and only the
 * ACCESS RELATION is composed into the array at offset 0 -- no C
 * changes, and the composition, offset/unit + sum of index*stride/unit,
 * stays affine in any number of dimensions.
 *
 * Composed, the relations say what the memory says: idx is 4096 bytes
 * into the storage lo names, which is lo[1024], and the second loop
 * writes it at iteration 1024 -- long after the read at iteration 0.
 * Uncomposed, pet sees three unrelated arrays, nothing orders the read
 * against the write, the loops fuse and the read is carried past the
 * write.  Measured, with the composition step disabled in
 * expr_collect_access:
 *
 *     -  out[0] = idx[0];
 *     -  for (int c0 = 0; c0 <= 4095; c0 += 1) {
 *     +  for (int c0 = 0; c0 <= 4095; c0 += 1) {
 *     ...
 *     +  out[0] = idx[0];
 *
 * and this program returns EXIT_FAILURE.  With it, EXIT_SUCCESS.
 *
 * EVERY ARRAY HERE IS float ON PURPOSE.  The graph this came from reads
 * that offset as int32_t, and written that way the test passes without
 * the fix: lo is written through float lvalues and idx read through an
 * int32_t one, so a compiler may assume the two do not alias and hoist
 * the read back above the loop it was wrongly scheduled after.  The
 * defect would then be masked by undefined behaviour rather than caught.
 * One type throughout makes the aliasing the standard's and not the
 * optimiser's to disbelieve.
 */
static float storage[8192];
static float s[4096];

void f(float * lo, float * hi, const float * idx, const float * s,
       float * out)
{
#pragma ppcg arena lo 0 idx 4096 hi 16384
#pragma scop
	for (int i = 0; i < 4096; i++) hi[i] = s[i];
	out[0] = idx[0];
	for (int i = 0; i < 4096; i++) lo[i] = hi[i] + 1.0f;
#pragma endscop
}

int main()
{
	float *lo = &storage[0];
	const float *idx = &storage[1024];
	float *hi = &storage[4096];
	float out[1];

	for (int i = 0; i < 4096; i++)
		s[i] = i + 1.0f;
	for (int i = 0; i < 4096; i++)
		lo[i] = 0.0f;
	storage[1024] = 7.0f;

	out[0] = 0.0f;
	f(lo, hi, idx, s, out);

	if (out[0] != 7.0f)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
