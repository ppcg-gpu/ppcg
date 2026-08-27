#include <stdint.h>

/* AN ANTI-DEPENDENCE BETWEEN TWO MEMBERS OF ONE STORAGE.
 *
 * The representative is not at offset zero: `lo` starts 256 bytes below
 * `h` and `idx` 192 below it, so `idx[0]` and `lo[16]` are the same four
 * bytes.  The read of `idx[0]` happens before the loop that writes `lo`,
 * so there is an anti-dependence from that read to the write at i = 16,
 * and it is the only thing that stops the two from being fused.
 *
 * BOTH ENDS OF IT USED TO BE THROWN AWAY BEFORE dep_false WAS COMPUTED,
 * by the same call, for two different reasons, and neither said so:
 *
 *   the extent did not reach.  pet gives every array the natural
 *   universe, "h[i0] : i0 >= 0", while an annotation may put the
 *   representative anywhere in the storage -- so a member composes onto
 *   a NEGATIVE index and intersect_range removes the access whole.  The
 *   extent is now the annotation's, taken down to the lowest element any
 *   member reaches.
 *
 *   the id did not match.  isl matches tuples by the id OBJECT, and two
 *   ids can print the same name.  pet_expr_anonymize strips the
 *   ValueDecl from every relation before the scop is handed over; the
 *   arena composition was not in that walk, so the composed access
 *   carried an id printing "h" and holding the decl while the extent
 *   carried one printing "h" and holding NULL.
 *
 * Either way the accesses to `lo` and `idx` are not misplaced but
 * ABSENT, dep_false comes out empty, and nothing forbids the fusion.
 *
 * This is the probe llama-dspark calls `aneg`, at a sixty-fourth of its
 * size: the proportions are what matters -- the representative 4*HALF
 * bytes above `lo` and 3*HALF above `idx` -- and every rung from
 * HALF=4096 down to HALF=64 was measured to discriminate alike, the
 * anti-dependence landing at i = HALF/4 each time.  The companion .deps
 * does not name that index: it moves with the size, and an expectation
 * pinned to it would be brittle where the claim is not.
 */
void f(uint16_t * h, float * lo, const int32_t * idx, const float * s,
       int32_t * out) {
#pragma ppcg arena h 0 lo -256 idx -192
#pragma scop
    for (int i = 0; i < 64; i++) h[i] = (uint16_t) s[i];
    out[0] = idx[0];
    for (int i = 0; i < 64; i++) lo[i] = (float) h[i] + 1.0f;
#pragma endscop
}
