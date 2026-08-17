#include <stdio.h>

#define N 1024

void iterator(float a[N], float *pw, int *pc);

int main(void)
{
	static float a[N];
	float weighted;
	int counted;
	int i;

	for (i = 0; i < N; ++i)
		a[i] = (float) (1 + (i * 37) % 101) / 70000.0f;

	iterator(a, &weighted, &counted);
	printf("%.6f %d\n", weighted, counted);

	return 0;
}
