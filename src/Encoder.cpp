#include "Encoder.h"
#include "Tasks.h"
#include "Shell.h"

static void enskip(Buffer &send, ShellEvent &se, void *data) {
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

	static ShellCallback callbacks[] = {
		{ F("enskip"), enskip, this },
	};

	shell.add(callbacks, ARRSIZE(callbacks));

	return sizeof(Encoder) + sizeof(callbacks);
}


