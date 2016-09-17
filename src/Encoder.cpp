#include "Encoder.h"
#include "Tasks.h"
#include "Broker.h"

static void enskip(Buffer &send, BrokerEvent &be, void *data) {
	((Encoder *)data)->skipEncoder();
}

static uint32_t runEncoder(uint32_t t0, void *data) {
	extern void simTimerEncoder(uint32_t ticks);
	int32_t ticks = encoder.run(t0);
	//simTimerEncoder(ticks);
	return ticks;
}

uint16_t Encoder::init() {
	ratio = 0;
	rpm = 0;
	edges = 0;
	teeth = 0;
	edge = 0;
	miss = 0;
	pulse = MicrosToTicks(65535U);

	taskmgr.addTask(F("Encoder"), runEncoder, 0, MicrosToTicks(refresh()));

	broker.add(enskip, this, F("enskip"));

	return sizeof(Encoder);
}


