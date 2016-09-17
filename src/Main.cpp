#include "Tasks.h"
#include "System.h"
#include "Schedule.h"
#include "Events.h"
#include "Stream.h"
#include "Hardware.h"

DEBUGINIT();

#ifdef ARDUINO
void Hardware::flush() {
	Buffer &send = hardware.send();
	Buffer &recv = hardware.recv();

	while (Serial.available())
	recv.putb(Serial.read());

	char *buf[32];
	size_t n = 0;

	if ((n = send.read(buf, sizeof(buf))) > 0)
	send.write(buf, n);
}
#endif

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

	initSystem(false);

	TaskMgr::runTasks();
}

#else

void toggleled(uint8_t id) {
}

#ifdef linux

#include <stdio.h>
#include <time.h>
#include <unistd.h>

void Hardware::flush() {
	char buf[32];
	size_t n = 0;

	extern int available(int fd);

	while (available(0) > 0)
		if ((n = read(0, buf, 1)) > 0)
			hardware.recv(0).write(buf, n);

	while ((n = hardware.send(0).read(buf, sizeof(buf))) > 0) {
		n = fwrite(buf, 1, n, stdout);
		fflush(stdout);
	}
}

static void stabilize(uint16_t ms) {
	uint32_t start = clockTicks();

	while (clockTicks() - start < ms * 1000)
		BitSchedule::runSchedule(clockTicks(), 0);
}

static void runDecoderTest() {
	BitPlan bp;

	bzero(&bp, sizeof(bp));

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

static void runTimeTest() {
	assert(tdiff32(1, (1L << 32) - 1) == 2);
	assert(tdiff32(10, (1L << 32) - 10) == 20);
	assert(tdiff32(10, 2) == 8);
	assert(tdiff32(2, 10) == -8);

	//assert(uelapsed32(1, (1L << 32) - 1) == 2);
	//assert(uelapsed32(10, (1L << 32) - 10) == 20);
	//assert(uelapsed16(10, (1L << 16) - 10) == 20);
	//assert(uelapsed32(10, 2) == 8);
	//assert(uelapsed16(10, 2) == 8);
	//assert(uelapsed32(2, (1L << 32) - 6) == 8);
	//assert(uelapsed16(2, (1 << 16) - 3) == 5);

	printf("%10u %10u\n", TicksToMicros(clockTicks()), clockTicks());
	delayTicks(MicrosToTicks(500 * 1000));
}

int main(int argc, char **argv) {
	bool doperfect = true;
	bool efi = true;
	int opt;

	while ((opt = getopt(argc, argv, "dep")) >= 0) {
		switch (opt) {
		case 'p':
			doperfect = !doperfect;
			break;
		case 'd':
			runDecoderTest();
			break;
		case 't':
			runTimeTest();
			break;
		case 'e':
			efi = !efi;
			break;
		default:
			fprintf(stderr, "Usage %s [-p port|-b baud|-s script|-c command -q -v]\n", argv[0]);
			exit(1);
		}
	}

	initSystem(efi);

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

	if (doperfect) {
		printf("Running perfect time!\n");
		extern uint32_t perfectus;
		perfectus = 1000;
	} else
		printf("Running..\n");

	while (true) {
		uint32_t us = taskmgr.check();

		if (us > 0)
			delayus(us);
	}
}

#endif
#endif
