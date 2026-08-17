#define N 1024
float total(float a[N])
{
	float sum = 0.0f;
#pragma scop
	for (int i = 0; i < N; ++i)
		sum += a[i];
#pragma endscop
	return sum;
}
