/* WHAT THE WALK KEEPS THAT THE CLOSURE DROPPED.
 *
 * Dead code elimination used to keep exactly the statement INSTANCES
 * that can reach a root, by taking the backward image of the roots under
 * the transitive closure of dep_flow.  That is the precise answer and it
 * does not survive a whole program -- on 668 nodes the closure took a
 * union map of 2765 relations to 1184629 at forty gigabytes and was
 * still growing.  The walk goes over STATEMENTS instead: one vertex per
 * tuple name, one edge per map of dep_flow, breadth first backwards, and
 * every instance of a reached statement is kept.
 *
 * That is an overapproximation, and this file is the smallest shape of
 * it.  Sixteen values are written, thirteen are read.  The closure sees
 * that a[13], a[14] and a[15] reach nothing and drops those three
 * instances; the walk sees that the statement writing a reaches a root
 * and keeps all sixteen.
 *
 * Measured, the same file through both formulations.  Under the walk the
 * writing loop runs to fifteen and the read is put under a condition
 * that stops at twelve; under the closure the loop itself stops at
 * twelve and the read needs no condition.  The two shapes are NOT
 * written out here in the spelling the generated code uses: ppcg copies
 * this comment into its output, and an expectation that its own
 * commentary can satisfy pins nothing.  Written that way once, this cell
 * passed against BOTH formulations.
 *
 * THE DIRECTION IS THE WHOLE POINT.  More kept than needed costs a
 * missed elimination; less kept than needed is a wrong program.  This
 * file pins the sound direction: should the walk ever be sharpened into
 * something that drops an instance, the bound goes back to twelve and
 * this test says so.  It is read rather than run because a program
 * cannot tell -- nobody reads the last three values either way.
 */
void f(float * o)
{
	float a[16];
#pragma scop
	for (int i = 0; i < 16; i++)
		a[i] = i;
	for (int i = 0; i < 13; i++)
		o[i] = a[i];
#pragma endscop
}
