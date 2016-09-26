#include "Encoder.h"
#include "Tasks.h"
#include "Broker.h"
#include "Timers.h"

static void enskip(Buffer &send, BrokerEvent &be, void *data) {
	((Encoder *)data)->skipEncoder();
}

static uint32_t runEncoder(uint32_t t0, void *data) {
	int32_t ticks = encoder.run(t0);
	//Timer::simTimer(TimerId2, ticks);
	return ticks;
}

static uint32_t timercb(TimerId id, uint32_t now, void *data) {
	return now + encoder.run(now);
}

void Encoder::init() {
	ratio = 0;
	rpm = 0;
	edges = 0;
	teeth = 0;
	edge = 0;
	miss = 0;
	pulse = MicrosToTicks(65535U);

	broker.add(enskip, this, F("enskip"));

	if (!Timers::initTimer(TimerId2, timercb, 0))
		taskmgr.addTask(F("Encoder"), runEncoder, 0, MicrosToTicks(refresh()));
}

uint16_t Encoder::mem(bool alloced) {
	return 0;
}


