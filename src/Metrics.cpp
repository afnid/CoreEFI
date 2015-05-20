// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Metrics.h"

void Metric::init() {
	ticks = clockTicks();
}

void Metric::calc() {
	ticks = TicksToMicros(clockTicks() - ticks);
}

void Metric::send(const Metric *metrics, uint8_t count, uint16_t over) {
	int n = 0;

	for (uint8_t i = 0; i < count; i++)
		if (metrics[i].ticks > over) {
			if (!n++)
				channel.p1(F("metrics"));
			channel.send(i, (int16_t) metrics[i].ticks);
		}

	if (n)
		channel.p2();
}

void sendHist(uint16_t *hist, uint8_t count) {
	int n = 0;

	for (int i = 0; i < count; i++) {
		if (!n++)
			channel.p1(F("hist"));
		channel.send(i, hist[i], false);
		hist[i] = 0;
	}

	if (n)
		channel.p2();
}

void sendHist(volatile uint16_t *hist, uint8_t count) {
	int n = 0;

	for (int i = 0; i < count; i++) {
		if (!n++)
			channel.p1(F("hist"));
		channel.send(i, hist[i], false);
		hist[i] = 0;
	}

	if (n)
		channel.p2();
}
