// copyright to me, released under GPL V3

#include <string.h>

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"
#include "Prompt.h"
#include "Tasks.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#if 0
#include "Decoder.h"
#include "Encoder.h"
#include "Events.h"
#include "Schedule.h"
#include "Strategy.h"
#include "Timers.h"
#endif

static PromptCallback *head = 0;

static void get(const char *s) {
	while (*s == ' ')
		s++;

	ParamTypeId id = (ParamTypeId)atoi(s);

	if (id >= 0) {
		channel.p1(F("ack"));
		channel.send(id, getParamFloat(id));
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
		ParamTypeId id = (ParamTypeId)atoi(s);
		float val = atof(a);
		setParamFloat(id, val);

		//channel.p1(F("id"));
		//channel.send(F("n"), id);
		//channel.send(F("v"), val);
		//channel.p2();
		//channel.nl();
	}
}

static int dumbstrcmp(const char *s1, const channel_t *s2) {
#ifdef ARDUINO
	const uint8_t *src = (uint8_t *)s2;
	char ch = 0;

	while (*s1) {
		ch = pgm_read_byte_near(src++);

		if (*s1 != ch)
			return *s1 - ch;

		s1++;
	}

	return 0;
#else
	return strcmp(s1, s2);
#endif
}

static char cmdbuf[20];

void handleInput(char ch) {
	if (ch == '\n' || ch == '\r') {
		bool found = false;

		for (PromptCallback *cb = head; cb; cb = cb->next) {
			if (!dumbstrcmp(cmdbuf, cb->key)) {
				cb->callback(cb->data);
				found = true;
				break;
			}
		}
		
		if (!found) {
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
						extern void sendBenchmarks();
						sendBenchmarks();
						break;

					case 'k':
						sendTicks();
						break;
					case 'm':
						setParamUnsigned(FlagIsMonitoring, isParamSet(FlagIsMonitoring) ? 0 : 5000);
						break;

					case 'z':
#ifdef UNIX
						exit(0);
#endif
						break;
					case '?':
						channel.send(F("Usage: u <id> <val>, q <id>, [cdehrstplvd]\n"));
						channel.send(F("\tm - toggle monitoring\n"));
						channel.send(F("\tu - updates a param, e.g. u 0 5000 sets the decoder rpm to 5k\n"));
						channel.send(F("\tq - query a param\n"));

						for (PromptCallback *cb = head; cb; cb = cb->next) {
							channel.send(cb->key);

							if (cb->desc) {
								channel.send(F(" - "));
								channel.send(cb->desc);
							}

							channel.nl();
						}

						channel.send(F("\tMost stats are reset every time they are displayed\n"));

						break;
					default:
						channel.send(F("? for help, Received"), *s);
						channel.nl();
						break;
				}
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

static uint32_t runInput(uint32_t now, void *data) {
#ifdef ARDUINO
	for (int8_t i = 0; i < 20 && Serial.available(); i++)
		handleInput(Serial.read());
#else
	bool available(int fd);
	char readch(int fd);
	char ch;

	for (int8_t i = 0; i < 20 && (ch = available(0)); i++)
		handleInput(readch(0));
#endif

	return 0;
}

uint16_t initPrompt() {
	myzero(cmdbuf, sizeof(cmdbuf));
	TaskMgr::addTask(F("Prompt"), runInput, 0, 400);
	return sizeof(cmdbuf);
}

void addPromptCallbacks(PromptCallback *callbacks, uint8_t ncallbacks) {
	for (uint8_t i = 0; i < ncallbacks; i++) {
		callbacks->next = head;
		head = callbacks++;
	}
}

#ifdef STM32

#include "st_main.h"

bool available(int fd) {
	extern bool VCP_avail();
	return VCP_avail();
}

char readch(int fd) {
	extern char VCP_getchar();
	return VCP_getchar();
}

#elif !defined(ARDUINO)

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/time.h>
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

#endif

