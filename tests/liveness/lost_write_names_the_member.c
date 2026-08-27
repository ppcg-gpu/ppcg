/* WHICH ARRAY IS TOLD IT LOST A WRITE.
 *
 * "arrays losing a write" used to be read from the COMPOSED may_writes,
 * so a dead statement writing through a member of an arena was charged
 * to the storage's representative -- a name the source never wrote and
 * which loses nothing, because the member is in the same report, judged
 * correctly, and the representative's entry is that member's write
 * wearing another name.  Liveness itself has been computed over the
 * plain relations all along; only the report was not.
 *
 * On the 402-node graph that phantom put three representatives among the
 * losses, with 90, 18 and 4 composed writes against 2, 2 and 1 of their
 * own, and made the report unreadable on the one question it exists to
 * answer: does an array the annotation names lose a write.
 *
 * Here mem is a member of rep's storage and nothing reads what it is
 * given.  Neither is live out -- both are local -- so the write dies,
 * and the report has to name the array the source wrote.  Read from the
 * composed relations it names the other one instead.
 *
 * The claim is a NAME ppcg PRINTS, not a line it generates, so it is a
 * .says rather than an .expect: a reader acts on the name, and a report
 * that is honest about arrays nobody declared is no report at all.
 */
void f(float * o)
{
	float rep[2048];
	float mem[16];
#pragma ppcg arena rep 0 mem 4096
#pragma scop
	for (int i = 0; i < 16; i++)
		mem[i] = i;
	for (int i = 0; i < 16; i++)
		o[i] = i;
#pragma endscop
}
