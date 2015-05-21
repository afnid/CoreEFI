// copyright to me, released under GPL V3

#include <string.h>

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Codes.h"
#include "Decoder.h"
#include "Encoder.h"
#include "Events.h"
#include "Pins.h"
#include "Schedule.h"
#include "Strategy.h"
#include "Tasks.h"
#include "Timers.h"

static void get(const char *s) {
	while (*s == ' ')
		s++;

	int id = atoi(s);

	if (id >= 0) {
		channel.p1(F("ack"));
		channel.send(id, getParamFloat(atoi(s)));
		channel.p2();
		channel.nl();
	}
}

static void set(char *s) {
	while (*s == ' ')
		s++;

	char *a = 0;

	for (char *t = s; !a && *t; t++)
		if (*t == ' ')
			a = t;

	if (a) {
		*a++ = 0;
		uint8_t id = atoi(s);
		float val = atof(a);
		setParamFloat(id, val);

		/*
		 channel.p1(F("id"));
		 channel.send(id);
		 channel.send(val);
		 channel.p2();
		 channel.nl();
		 */
	}
}

static char cmdbuf[12];

#include <math.h>

static uint32_t runBench1() {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;
	float a = 1.001;

	while (tdiff32(clockTicks(), t1) < 1000000L) {
		a += 0.01f * sqrtf(a);
		count++;
	}

	assert(a > 0 || a <= 0);

	return count;
}

static uint32_t runBench2() {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;
	double a = 1.001;

	while (tdiff32(clockTicks(), t1) < 1000000L) {
		a += (double)0.01 * sqrt(a);
		count++;
	}

	assert(a > 0 || a <= 0);

	return count;
}

static uint32_t runBench3() {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;
	uint32_t a = 1;

	while (tdiff32(clockTicks(), t1) < 1000000L) {
		a = ((a * a) >> 2) + 2001481UL;
		count++;
	}

	assert(a > 0 || a <= 0);

	return count;
}

static uint32_t runBench4() {
	uint32_t t1 = clockTicks();
	uint32_t count = 0;

	while (tdiff32(clockTicks(), t1) < 1000000L)
		count++;

	return count;
}

#ifdef ARDUINO
#include <Arduino.h>

static uint32_t runBench5() {
	//extern "C" uint32_t micros();
	extern uint32_t clockTicks2();
	uint32_t t1 = clockTicks2();
	uint32_t count = 0;

	while (tdiff32(clockTicks2(), t1) < 1000000L)
		count++;

	return count;
}
#else
static uint32_t runBench5() {
	return 0;
}
#endif

void handleInput(char ch) {
	if (ch == '\n' || ch == '\r') {
		for (char *s = cmdbuf; *s; s++) {
			switch (*s) {
				case 'q':
					get(s + 1);
					myzero(cmdbuf, sizeof(cmdbuf));
					break;
				case 'u':
					set(s + 1);
					myzero(cmdbuf, sizeof(cmdbuf));
					break;

				case 'b':
					channel.send(F("bench1"), runBench1());
					channel.send(F("bench2"), runBench2());
					channel.send(F("bench3"), runBench3());
					channel.send(F("bench4"), runBench4());
					channel.send(F("bench5"), runBench5());
					channel.nl();
					s++;
					break;

				case 'c':
					codes.send();
					break;
				case 'd':
					switch (s[1]) {
						case '0':
							decoder.sendStatus();
							s++;
							break;
						case '1':
							decoder.sendList();
							s++;
							break;
						default:
							decoder.sendStatus();
							decoder.sendList();
							break;
					}
					break;
				case 'e':
					switch (s[1]) {
						case '0':
							sendEventStatus();
							s++;
							break;
						case '1':
							sendEventList();
							s++;
							break;
						default:
							sendEventStatus();
							sendEventList();
							break;
					}
					break;
				case 'h':
					sendPins();
					break;
				case 's':
					sendSchedule();
					break;
				case 'i':
					sendTimers();
					break;
				case 't':
					sendTasks();
					break;
				case 'm':
					setParamUnsigned(FlagIsMonitoring, isParamSet(FlagIsMonitoring) ? 0 : 5000);
					break;

				case 'v':
					sendParamValues();
					break;
				case 'p':
					switch (s[1]) {
						case '0':
							sendParamLookups();
							s++;
							break;
						case '1':
							sendParamList();
							s++;
							break;
						case '2':
							sendParamStats();
							s++;
							break;
						case '3':
							sendParamChanges();
							s++;
							break;
						default:
							sendParamChanges();
							break;
					}
					break;
				case 'x':
					encoder.skipEncoder();
					break;
#ifdef STM32
				case 'y':
					extern void printTimers();
					printTimers();
					break;
#endif
				case 'z':
#ifdef UNIX
					exit(0);
#endif
					break;
				case '?':
					channel.send(F("Usage: u <id> <val>, q <id>, [cdehrstplvd]\n"));
					channel.send(F("\tm - toggle monitoring\n"));
					channel.send(F("\tc - codes\n"));
					channel.send(F("\td0 - decoder status\n"));
					channel.send(F("\td1 - decoder list\n"));
					channel.send(F("\te0 - event status\n"));
					channel.send(F("\te1 - event list\n"));
					channel.send(F("\th - pins\n"));
					channel.send(F("\ti - interrupts\n"));
					channel.send(F("\ts - schedule\n"));
					channel.send(F("\tt - tasks\n"));
					channel.send(F("\tv - param values\n"));
					channel.send(F("\tp0 - param lookups\n"));
					channel.send(F("\tp1 - param list\n"));
					channel.send(F("\tp2 - param stats\n"));
					channel.send(F("\tp3 - param changes\n"));
					channel.send(F("\tv - values\n"));
					channel.send(F("\tu - updates a param\n"));
					channel.send(F("\tq - query a param\n"));
					break;
				default:
					channel.send(F("? for help, Received"), *s);
					channel.nl();
					break;
			}
		}

		myzero(cmdbuf, sizeof(cmdbuf));
	} else {
		uint8_t i = strlen(cmdbuf);

		if (i < sizeof(cmdbuf) - 2) {
			char *s = cmdbuf + i;
			*s++ = ch;
			*s++ = 0;
		}
	}
}

uint16_t initPrompt() {
	myzero(cmdbuf, sizeof(cmdbuf));
	return sizeof(cmdbuf);
}
