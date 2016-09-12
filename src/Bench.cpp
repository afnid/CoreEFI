#include "System.h"
#include "Channel.h"

#include <math.h>

static const long TIME = MicrosToTicks(1000000L);

static void towers(int num, char frompeg, char topeg, char auxpeg) {
	if (num > 1) {
		towers(num - 1, frompeg, auxpeg, topeg);
		towers(num - 1, auxpeg, topeg, frompeg);
	}
}

static uint32_t runSqrt() {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;
	float a = 1.001;

	while (tdiff32(clockTicks(), t1) < TIME) {
		a += 0.01f * sqrtf(a);

		if (a != 0)
			count++;
	}

	assert(a > 0 || a <= 0);

	return count;
}

static uint32_t runSin() {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;

	while (tdiff32(clockTicks(), t1) < TIME) {
		for (int i = 0; i < 360; i++) {
			float x = i * (3.1415927f / 180);
			float term = x;
			float sinx = term;
			int n = 1;

			do {
				float denominator = 2 * n * (2 * n + 1);
				term = -term * x * x / denominator;
				sinx += term;
			} while (n++ < 5);

			if (sinx >= -10)
				count++;
		}
	}

	return count;
}

static uint32_t runArea(int r) {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;

	while (tdiff32(clockTicks(), t1) < TIME) {
		for (int y = 0; y < 2 * r; y++) {
			for (int x = 0; x < 2 * r; x++) {
				int xx = r - x;
				int yy = r - y;
				int rr = xx * xx + yy * yy;

				if (abs(rr - r * r) < 5)
					count++;
			}
		}
	}

	return count;
}

static uint32_t runHanoi(int discs) {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;

	while (tdiff32(clockTicks(), t1) < TIME) {
		towers(discs, 'A', 'B', 'C');
		count++;
	}

	return count;
}

static uint32_t runTicks() {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;

	while (tdiff32(clockTicks(), t1) < TIME)
		count++;

	return count;
}

static uint32_t runTicks2() {
	extern uint32_t clockTicks2();
	uint32_t t1 = clockTicks2();
	uint32_t count = 0;

	while (tdiff32(clockTicks2(), t1) < 1000000L)
		count++;

	return count;
}

void sendBenchmarks() {
	channel.p1(F("benchmarks"));
	channel.send(F("sqrt"), runSqrt());
	channel.send(F("sin"), runSin());
	channel.send(F("area"), runArea(20));
	channel.send(F("hanoi"), runHanoi(8));
	channel.send(F("ticks"), runTicks());
	channel.send(F("ticks2"), runTicks2());
	channel.nl();
}
