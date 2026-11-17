#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lz4kdr.h"

static uint64_t rng = 88172645463325252ULL;

static uint8_t rnd(void)
{
	rng ^= rng << 13;
	rng ^= rng >> 7;
	rng ^= rng << 17;
	return (uint8_t)rng;
}

static void fill(uint8_t *p, unsigned n, int kind)
{
	for (unsigned i = 0; i < n; i++) {
		switch (kind) {
		case 0: p[i] = 0; break;
		case 1: p[i] = rnd(); break;
		case 2: p[i] = "the quick brown fox jumps "[i % 26]; break;
		case 3: p[i] = (uint8_t)(i % (1 + (i >> 9) % 15)); break;
		default: p[i] = (rnd() & 7) ? p[i ? i - 1 : 0] : rnd(); break;
		}
	}
}

static int failures;

static void check(const uint8_t *in, unsigned n, const char *what)
{
	static uint8_t state[1 << 16];
	uint8_t *out = malloc(2 * n + 128), *back = malloc(n);
	int c = lz4kdr_encode(state, in, out, n, 2 * n + 128, 2 * n + 128);

	if (c > 0) {
		int d = lz4kdr_decode(out, back, (unsigned)c, n);
		if (d != (int)n || memcmp(back, in, n)) {
			printf("FAIL %s size %u: encoded %d decoded %d\n", what, n, c, d);
			failures++;
		}
		uint8_t *wide = malloc(4096);
		d = lz4kdr_decode(out, wide, (unsigned)c, 4096);
		if (d != (int)n || memcmp(wide, in, n)) {
			printf("FAIL %s size %u: encoded %d decoded into 4096 %d\n", what, n, c, d);
			failures++;
		}
		free(wide);
		for (int k = 0; k < 64; k++) {
			out[rnd() % (unsigned)c] ^= (uint8_t)(1 + rnd() % 255);
			lz4kdr_decode(out, back, (unsigned)c, n);
		}
	} else if (c < 0) {
		printf("FAIL %s size %u: encode returned %d\n", what, n, c);
		failures++;
	}
	free(out);
	free(back);
}

int main(void)
{
	static const char *names[] = { "zero", "random", "text", "periodic", "runs" };
	unsigned sizes[] = { 1, 2, 7, 8, 9, 15, 16, 17, 31, 64, 100, 511, 1000, 2048, 4095, 4096 };

	for (int kind = 0; kind < 5; kind++)
		for (unsigned s = 0; s < sizeof(sizes) / sizeof(*sizes); s++)
			for (int rep = 0; rep < 20; rep++) {
				uint8_t *p = malloc(sizes[s]);
				fill(p, sizes[s], kind);
				check(p, sizes[s], names[kind]);
				free(p);
			}
	for (unsigned period = 1; period <= 16; period++)
		for (unsigned n = 1; n <= 4096; n++) {
			uint8_t *p = malloc(n);
			for (unsigned i = 0; i < n; i++)
				p[i] = i < period ? rnd() : p[i - period];
			check(p, n, "short period");
			free(p);
		}
	printf("%s, %d failures\n", failures ? "FAILED" : "ok", failures);
	return failures != 0;
}
