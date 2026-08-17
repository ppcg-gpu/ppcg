#include <stdio.h>

#define N 1024
#define B 16

void scatter(int idx[N], float a[N], float h[B]);

int main(void)
{
	static int idx[N];
	static float a[N];
	float h[B];
	int i;

	for (i = 0; i < N; ++i) {
		idx[i] = (i * 37) % B;
		a[i] = (float) (1 + (i * 37) % 101) / 7.0f;
	}
	for (i = 0; i < B; ++i)
		h[i] = 0.0f;

	scatter(idx, a, h);

	for (i = 0; i < B; ++i)
		printf("%.6f\n", h[i]);

	return 0;
}
