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
		channel.send(id, getParamDouble(atoi(s)));
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
		double val = atof(a);
		setParamDouble(id, val);

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
				case 'z':
					exit(0);
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
