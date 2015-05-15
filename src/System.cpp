#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"
#include "Free.h"

#include "Codes.h"
#include "Decoder.h"
#include "Encoder.h"
#include "Events.h"
#include "Pins.h"
#include "Prompt.h"
#include "Schedule.h"
#include "Strategy.h"
#include "Tasks.h"
#include "Timers.h"

#ifdef ARDUINO
#include <HardwareSerial.h>
//#include <SoftSerial.h>
#include <Arduino.h>
#else
Print Serial;
#endif

volatile Decoder decoder;
Codes codes;
Encoder encoder;
Channel channel(Serial);

static uint16_t add(uint16_t &total, uint16_t mem)
{
	total += mem;
	return mem;
}

void initSystem()
{
	uint16_t total = 0;
	Serial.begin(57600);
	Serial.println("CoreEFI v007a");

	channel.p1(F("mem"));
	channel.send(F("prompt"), add(total, initPrompt()));
	channel.send(F("codes"), add(total, codes.init()));
	channel.send(F("strategy"), add(total, initStrategy()));
	channel.send(F("encoder"), add(total, encoder.init()));
	channel.send(F("decoder"), add(total, decoder.init()));
	channel.send(F("schedule"), add(total, initSchedule()));
	channel.send(F("events"), add(total, initEvents()));
	channel.send(F("params"), add(total, initParams()));
	channel.send(F("tasks"), add(total, initTasks()));
	channel.send(F("pins"), add(total, initPins()));
	channel.send(F("timers"), add(total, initTimers()));
	channel.send(F("bytes"), total);
	channel.p2();
	channel.nl();

	setParamUnsigned(ConstCylinders, 1);

	channel.p1(F("max"));
	channel.send(F("cyls"), MaxCylinders);
	channel.send(F("coils"), MaxCoils);
	channel.send(F("teeth"), MaxEncoderTeeth);
	channel.send(F("cyls"), getParamUnsigned(ConstCylinders));
	channel.send(F("coils"), getParamUnsigned(ConstCoils));
	channel.send(F("teeth"), getParamUnsigned(ConstEncoderTeeth));
	channel.send(F("int"), (uint16_t)sizeof(int));
	channel.send(F("pulse"), (uint16_t)sizeof(pulse_t));
	channel.send(F("ustoticks"), (uint16_t) MicrosToTicks(1u));
	channel.p2();
	channel.nl();

	sendMemory();

#if defined(ARDUINO) || defined(STM32)
	setParamUnsigned(ConstTicksInUS, 1);
#else
	setParamUnsigned(ConstTicksInUS, 10);
#endif

	setParamUnsigned(SensorDEC, 22111);
	setParamUnsigned(SensorDEC, 1000);
	setParamUnsigned(SensorDEC, 3000);
	setParamUnsigned(SensorDEC, getParamUnsigned(ConstIdleRPM) / 2);

	setParamDouble(SensorHEGO1, 1);
	setParamDouble(SensorHEGO2, 1);
	setParamDouble(SensorECT, 190);
	setParamDouble(SensorACT, 150);
	setParamDouble(SensorMAF, 5);
	setParamDouble(SensorTPS, 20);
	setParamDouble(SensorVCC, 14);
	setParamUnsigned(SensorGEAR, 5);
	setParamUnsigned(SensorIsKeyOn, 1);

#ifdef NDEBUG
	assert(0);
#endif
}

void runInput()
{
	for (int8_t i = 0; i < 20 && Serial.available(); i++)
		handleInput(Serial.read());
}

#include <string.h>

void myzero(void *p, uint16_t len) {
	memset(p, 0, len);
}

#ifdef STM32

uint32_t micros() {
	extern volatile uint32_t time_var2;
	return time_var2;
}

void _delay_us(uint16_t ticks) {
}

bool available(int fd) {
	extern bool VCP_has_char();
	return VCP_has_char();
}

char readch(int fd) {
	extern int VCP_get_char(uint8_t *buf);
	uint8_t ch = 0;
	return VCP_get_char(&ch) == 1 ? ch : 0;
}

#else
#ifndef ARDUINO

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/time.h>

#define NanosToTicks(x) ((x) / (1000 / TICKTOUS))
#define TicksToNanos(x) ((x) * (1000 / TICKTOUS))

uint32_t clock_ticks() {
	static uint32_t offset = (1L << 32) - 5;
	static uint32_t sec = 0;
	static struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	if (sec == 0)
		sec = ts.tv_sec;

	ts.tv_sec -= sec;
	ts.tv_sec += offset;

	return NanosToTicks(ts.tv_sec * 1000000000ULL + ts.tv_nsec);
}

//uint32_t micros() {
	//return TicksToMicros(clock_ticks());
//}

void delay_ticks(uint32_t ticks) {
	static struct timespec req;
	static struct timespec rem;

	myzero(&req, sizeof(req));
	myzero(&rem, sizeof(rem));
	req.tv_nsec = TicksToNanos(ticks);
	req.tv_sec = req.tv_nsec / 1000000000ULL;
	req.tv_nsec -= req.tv_sec * 1000000000ULL;

	do {
		nanosleep(&req, &rem);
	} while (rem.tv_sec > 0 || rem.tv_nsec > 0);
}

void _delay_us(uint16_t ticks) {
	if (ticks < 100)
		while (ticks)
			--ticks;

	if (ticks > 0)
		delay_ticks(MicrosToTicks(ticks));
}

#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>

bool available(int fd) {
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	select(fd + 1, &fds, NULL, NULL, &tv);
	return (FD_ISSET(0, &fds));
}

char readch(int fd) {
	if (!available(fd))  // getting false input indicators
		return 0;
	char ch = 0;
	int n = read(fd, &ch, sizeof(ch));
	return n == 1 ? ch : 0;
}

static void stabilize(uint16_t ms) {
	int start = clock_ticks();

	while (clock_ticks() - start < ms * 1000)
		runSchedule();
}

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
	assert(tdiff32(1, (1L << 32) - 1) == 2);
	assert(tdiff32(10, (1L << 32) - 10) == 20);
	assert(tdiff16(10, (1L << 16) - 10) == 20);
	assert(tdiff32(10, 2) == 8);
	assert(tdiff16(10, 2) == 8);
	assert(tdiff32(2, 10) == -8);
	assert(tdiff16(2, 10) == -8);

	while (true) {
		printf("%10u %10u\n", TicksToMicros(clock_ticks()), clock_ticks());
		delay_ticks(MicrosToTicks(500 * 1000));
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

	initSystem();

	if (false)
		for (int i = 0; i < 0; i++) {
			logFine("b %d %ld %d", i, time(0), clock_ticks());
			_delay_us(1);
			logFine("a %d %ld %d", i, time(0), clock_ticks());
			_delay_us(50 * 1000);
		}

	if (false)
		for (double i = 0; i <= 5; i += 0.125) {
			setParamDouble(SensorMAF, i);
			stabilize(10);
			logFine("maf %10g %10g %10g %10g", i, getParamDouble(FuncMafTransfer), getParamDouble(CalcVolumetricRate), getParamDouble(TableInjectorTiming));
		}

	if (false)
		for (int i = getParamUnsigned(ConstIdleRPM) / 2; i <= 6000; i += 1500) {
			setParamDouble(SensorDEC, i);
			stabilize(10 + 60000 / i);
			logFine("final %10d %10d %10g %10g %10g %10g", i, getParamUnsigned(SensorRPM), getParamDouble(CalcFinalPulseAdvance), getParamDouble(CalcFinalSparkAdvance), getParamDouble(CalcFinalPulseWidth), getParamDouble(CalcFinalSparkWidth));
		}

	if (false) {
		printf("Running perfect time!\n");
		extern void runPerfectTasks();
		runPerfectTasks();
	} else {
		printf("Running..\n");
		runTasks();
	}
}

#endif
#endif
