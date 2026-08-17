#include <stdio.h>

#define B 16

void aliased(float h[2 * B]);

int main(void)
{
	float h[2 * B];
	int i;

	for (i = 0; i < 2 * B; ++i)
		h[i] = (float) ((i * 7) % 5) / 4.0f;

	aliased(h);

	for (i = 0; i < B; ++i)
		printf("%.6f\n", h[i]);

	return 0;
}
