/* The expanded spelling, run rather than only recognised.
 *
 * The detection test beside this one says the accumulation is found.
 * This one says the parallel code computes what the sequential code
 * computes -- which is the question that matters, because a reduction
 * clause on a loop that is not one gives a plausible wrong number
 * rather than a diagnostic.
 */
#define N 1024

float total(float a[N])
{
	float sum = 0.0f;
#pragma scop
	for (int i = 0; i < N; ++i)
		sum = sum + a[i];
#pragma endscop
	return sum;
}
