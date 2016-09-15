// copyright to me, released under GPL V3

#define EXTERN

#include "System.h"
#include "Stream.h"
#include "Metrics.h"
#include "Params.h"
#include "Free.h"

#include "Hardware.h"
#include "Tasks.h"
#include "Epoch.h"

#include "Codes.h"
#include "Decoder.h"
#include "Encoder.h"
#include "Events.h"
#include "Pins.h"
#include "Prompt.h"
#include "Schedule.h"
#include "Strategy.h"
#include "Timers.h"

#include "Display.h"
#include "Vehicle.h"
#include "Bus.h"

#ifdef ARDUINO
#include <HardwareSerial.h>
//#include <SoftSerial.h>
#include <Arduino.h>
#endif

#if 1

Buffer channel;
//Stream channel(Serial);
Display display;
CanBus bus;

#define EFI

#ifdef EFI
volatile Decoder decoder;
Encoder encoder;
CycleCount cycle;
#endif

static uint16_t add(uint16_t &total, uint16_t mem) {
	total += mem;
	return mem;
}

void initSystem(Buffer &send, bool doefi) {
	uint16_t total = 0;

	send.p1(F("mem"));
	send.json(F("ticks"), add(total, initTicks()));
	send.json(F("cycle"), add(total, cycle.init()));
	send.json(F("tasks"), add(total, TaskMgr::initTasks()));
	send.json(F("clock"), add(total, Epoch::init()));
	send.json(F("prompt"), add(total, initPrompt()));
	send.json(F("codes"), add(total, codes.init()));
	send.json(F("params"), add(total, initParams()));

	send.json(F("display"), add(total, display.init()));
	send.json(F("vehicle"), add(total, vehicle.init()));

	if (doefi) {
		send.json(F("strategy"), add(total, initStrategy()));
		send.json(F("encoder"), add(total, encoder.init()));
		send.json(F("decoder"), add(total, decoder.init()));
		send.json(F("schedule"), add(total, initSchedule()));
		send.json(F("events"), add(total, initEvents()));
		send.json(F("timers"), add(total, initTimers()));
	}

	send.json(F("bytes"), total);
	channel.p2();
	channel.nl();

	send.p1(F("max"));
	send.json(F("cyls"), MaxCylinders);
	send.json(F("coils"), MaxCoils);
	send.json(F("teeth"), MaxEncoderTeeth);
	send.json(F("cyls"), getParamUnsigned(ConstCylinders));
	send.json(F("coils"), getParamUnsigned(ConstCoils));
	send.json(F("teeth"), getParamUnsigned(ConstEncoderTeeth));
	send.json(F("int"), (uint16_t) sizeof(int));
	send.json(F("long_int"), (uint16_t) sizeof(long int));
	send.json(F("long_long_int"), (uint16_t) sizeof(long long int));
	send.json(F("pulse"), (uint16_t) sizeof(pulse_t));
	send.json(F("ustoticks"), (uint16_t) MicrosToTicks(1u));
	channel.p2();
	channel.nl();

	sendMemory();

	setParamUnsigned(ConstTicksInUS, TICKTOUS);

	setParamUnsigned(SensorDEC, 22111);
	setParamUnsigned(SensorDEC, 1000);
	setParamUnsigned(SensorDEC, 3000);
	setParamUnsigned(SensorDEC, getParamUnsigned(ConstIdleRPM) / 2);

	setParamFloat(SensorHEGO1, 1);
	setParamFloat(SensorHEGO2, 1);
	setParamFloat(SensorECT, 190);
	setParamFloat(SensorACT, 150);
	setParamFloat(SensorMAF, 5);
	setParamFloat(SensorTPS, 20);
	setParamFloat(SensorVCC, 14);
	setParamUnsigned(SensorGEAR, 5);
	GPIO::setPin(GPIO::IsKeyOn, 1);

#ifdef EFI
	encoder.refresh();
	decoder.refresh(clockTicks());
#endif
}
#endif

#include <string.h>

void myzero(void *p, uint16_t len) {
	memset(p, 0, len);
}

#ifdef STM32

#include "st_main.h"

void toggleled(uint8_t id) {
#ifdef __STM32F4_DISCOVERY_H
	static Led_TypeDef leds[] = {
		LED3,
		LED4,
		LED5,
		LED6,
	};

	BSP_LED_Toggle(leds[id & 0x3]);
#endif
}

int main(int argc, char **argv) {
	global.init();

	initSystem(channel, false);

	TaskMgr::runTasks();
}

#else

void toggleled(uint8_t id) {
}

#ifndef ARDUINO

#include <time.h>

static void stabilize(uint16_t ms) {
	int start = clockTicks();

	while (clockTicks() - start < ms * 1000)
		runSchedule();
}

#include <stdio.h>

static void runDecoderTest() {
	BitPlan bp;

	myzero(&bp, sizeof(bp));

	for (uint32_t max = 10000; max <= 100000; max *= 10) {
		for (uint32_t ang = 0; ang <= 65536u; ang += 8192) {
			int32_t skip = max / 2;
			skip = 5000;

			printf("%10d %10u %10u", ang, max, skip);

			for (int32_t i = -skip; i <= skip; i += skip) {
				bp.setEvent(0, 0, 0, ang, i, 0);
				printf("%10d", bp.calcTicks(max));
			}

			printf("\n");
		}

		printf("\n");
	}
}

int main(int argc, char **argv) {
	extern void delayTicks(uint32_t ticks);

	assert(tdiff32(1, (1L << 32) - 1) == 2);
	assert(tdiff32(10, (1L << 32) - 10) == 20);
	assert(tdiff32(10, 2) == 8);
	assert(tdiff32(2, 10) == -8);

	while (true) {
		printf("%10u %10u\n", TicksToMicros(clockTicks()), clockTicks());
		delayTicks(MicrosToTicks(500 * 1000));
		break;
	}

	runDecoderTest();

	//assert(uelapsed32(1, (1L << 32) - 1) == 2);
	//assert(uelapsed32(10, (1L << 32) - 10) == 20);
	//assert(uelapsed16(10, (1L << 16) - 10) == 20);
	//assert(uelapsed32(10, 2) == 8);
	//assert(uelapsed16(10, 2) == 8);
	//assert(uelapsed32(2, (1L << 32) - 6) == 8);
	//assert(uelapsed16(2, (1 << 16) - 3) == 5);

	initSystem(channel, false);

	if (false)
		for (int i = 0; i < 0; i++) {
			logFine("b %d %ld %d", i, time(0), clockTicks());
			delayTicks(1);
			logFine("a %d %ld %d", i, time(0), clockTicks());
			delayTicks(50 * 1000);
		}

	if (false)
		for (float i = 0; i <= 5; i += 0.125f) {
			setParamFloat(SensorMAF, i);
			stabilize(10);
			logFine("maf %10g %10g %10g %10g", i, getParamFloat(FuncMafTransfer), getParamFloat(CalcVolumetricRate), getParamFloat(TableInjectorTiming));
		}

	if (false)
		for (int i = getParamUnsigned(ConstIdleRPM) / 2; i <= 6000; i += 1500) {
			setParamFloat(SensorDEC, i);
			stabilize(10 + 60000 / i);
			logFine("final %10d %10d %10g %10g %10g %10g", i, getParamUnsigned(SensorRPM), getParamFloat(CalcFinalPulseAdvance), getParamFloat(CalcFinalSparkAdvance),
					getParamFloat(CalcFinalPulseWidth), getParamFloat(CalcFinalSparkWidth));
		}

	if (argc > 1) {
		printf("Running perfect time!\n");
		extern void runPerfectTasks();
		runPerfectTasks();
	} else {
		printf("Running..\n");
		TaskMgr::runTasks();
	}
}

#endif
#endif
