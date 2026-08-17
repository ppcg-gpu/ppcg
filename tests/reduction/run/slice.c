#define N 200

/* An accumulation into part of a row of a two dimensional array.
 *
 * A reduction clause names a section, and a section of an array of more
 * than one dimension only is one when it covers the whole of every
 * dimension but the outermost.  Naming h[0:4][0:1] here would be
 * rejected by the compiler that has to build the generated code, so h
 * may not be named at all.
 */
void slice(float a[N][N], float h[4][4])
{
#pragma scop
	for (int i = 0; i < N; ++i)
		for (int j = 0; j < N; ++j)
			h[i % 4][0] += a[i][j];
#pragma endscop
}
