// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Decoder.h"
#include "Tasks.h"
#include "Encoder.h"
#include "Events.h"
#include "Free.h"
#include "Pins.h"
#include "Schedule.h"
#include "Strategy.h"
#include "Timers.h"

static inline void runClock(uint32_t seconds) {
	uint16_t rpm = getParamUnsigned(SensorRPM);
	uint16_t cranking = getParamUnsigned(ConstCrankingRPM);
	setParamUnsigned(TimeOnSeconds, seconds + 1);
	setTimer(TimeCrankSeconds, (rpm > 0 && rpm < cranking) || isParamSet(SensorIsCranking));
	setTimer(TimeRunSeconds, rpm >= cranking);
	setTimer(TimeOffSeconds, !isParamSet(SensorIsKeyOn));
	setTimer(TimeMovingSeconds, isParamSet(SensorVSS));
	setTimer(TimeIdleSeconds, getParamFloat(SensorTPS) <= 20);
}

static inline uint32_t runStatus() {
	uint32_t wait = getParamUnsigned(FlagIsMonitoring);

	toggleled(0);

	if (!wait) {
		sendEventStatus();
#ifdef ARDUINO
		//sendTimers();
#endif
		return MicrosToTicks(3000017UL);
	}

	decoder.sendList();
	sendEventList();

	wait = max(wait, 500);

	return MicrosToTicks(wait * 3000);
}

static inline uint32_t runChanges() {
	uint32_t wait = getParamUnsigned(FlagIsMonitoring);

	if (wait) {
		sendParamChanges();
		wait = max(wait, 500);
		wait = min(wait, 1000);
		return MicrosToTicks(wait);
	}

	return MicrosToTicks(3000017UL);
}

static inline void runRefresh(uint32_t now) {
	refreshEvents();
	encoder.refresh();
	decoder.refresh(now);
}

static inline void runTest() {
	for (uint16_t i = 0; i < 100; i++)
		getParamUnsigned(ConstEncoderTeeth);

	//setParamFloat(SensorVSS, getParamUnsigned(CalcRPMtoMPH));
	//getParamFloat(CalcFinalLamda);
	//getParamFloat(CalcMPG);
	//getParamFloat(CalcDutyCycle);
}

enum {
	TaskStatus,
	TaskChanges,
	TaskInput,
	TaskSchedule,

	TaskPins,
	TaskClock,
	TaskRefresh,

#if defined(ARDUINO) || defined(STM32)
	TaskMax,
	TaskEvents,
	TaskEncoder,
#else
	TaskEvents,
	TaskEncoder,
	TaskMax,
#endif

	// disabled
	TaskTest
};

typedef struct _Node {
	Task task;
	struct _Node *next;
	struct _Node *prev;

	inline void verify() {
		assert(next);
		assert(prev);
		assert(next->prev == this);
		assert(prev->next == this);
	}

	inline void unlink() {
		next->prev = prev;
		prev->next = next;
	}

	inline void append(struct _Node *n) {
		n->next = next;
		n->prev = this;
		next->prev = n;
		next = n;
		verify();
	}

	inline void insert(struct _Node *n) {
		n->next = this;
		n->prev = prev;
		prev->next = n;
		prev = n;
		verify();
	}

	void send(struct _Node *zero) {
		assert(this - zero >= 0);
		assert(this - zero < TaskMax);
		channel.p1(F("task"));
		channel.send(F("i"), (uint16_t) (this - zero));
		channel.send(F("n"), (uint16_t) (next - zero));
		channel.send(F("p"), (uint16_t) (prev - zero));
		channel.p2();
		verify();
	}
} Node;

static struct {
	Node queue[TaskMax];
	Node *head;
	uint32_t start;
	uint32_t slept;

	inline void verify(Node *h) {
		assert(h == head || tdiff32(h->task.getNext(), h->prev->task.getNext()) >= 0);
		assert(h->next == head || tdiff32(h->task.getNext(), h->next->task.getNext()) <= 0);
		h->verify();
	}

	inline void addTask(Node *n) {
		Node *h = head;

		while (tdiff32(n->task.getNext(), h->task.getNext()) >= 0 && h->next != head)
			h = h->next;

		if (tdiff32(h->task.getNext(), n->task.getNext()) > 0) {
			h->insert(n);

			if (h == head)
				head = n;
		} else
			h->append(n);

		verify(n);
	}

	inline void reschedule(Node *n) {
		if (n == head)
			head = n->next;
		n->unlink();
		addTask(n);
	}

	inline void setEpoch() {
		uint32_t now = clockTicks();

		for (uint8_t i = 0; i < TaskMax; i++)
			queue[i].task.setNext(now);

		start = now;
	}
} tasks;

void sendTasks() {
	uint32_t now = clockTicks();
	uint32_t us = TicksToMicros(now);
	Node *n = tasks.head;

	channel.p1(F("tasks"));
	channel.send(F("slept"), tasks.slept);
	channel.send(F("msticks"), us / 1000.0f);
	channel.p2();
	channel.nl();

	tasks.slept = 0;

	do {
		n->task.send(now);
		n = n->next;
	} while (n != tasks.head);
}

uint16_t initTasks() {
	myzero(&tasks, sizeof(tasks));

	for (uint8_t i = 0; i < TaskMax; i++) {
		tasks.queue[i].next = tasks.queue + i;
		tasks.queue[i].prev = tasks.queue + i;
		tasks.queue[i].task.init(1000, i);
	}

	// http://prime-numbers.org/prime-number-2000000-2005000.htm, less contention, but not necessary
	tasks.queue[TaskEvents].task.setWait(1);
	tasks.queue[TaskEncoder].task.setWait(MicrosToTicks(encoder.refresh()));

	tasks.queue[TaskSchedule].task.setWait(MicrosToTicks(23431UL));
	tasks.queue[TaskInput].task.setWait(MicrosToTicks(50341UL));
	tasks.queue[TaskPins].task.setWait(MicrosToTicks(51797UL));
	tasks.queue[TaskClock].task.setWait(MicrosToTicks(251263UL));
	tasks.queue[TaskRefresh].task.setWait(MicrosToTicks(2001481UL));
	tasks.queue[TaskStatus].task.setWait(MicrosToTicks(3000017UL));

	tasks.head = tasks.queue;

	for (uint8_t i = 1; i < TaskMax; i++)
		tasks.addTask(tasks.queue + i);

	tasks.setEpoch();

	return sizeof(tasks);
}

int32_t runTask(uint32_t now) {
	if (tasks.head->task.canRun(now)) {
		Node *n = tasks.head;

		tasks.reschedule(n);

		uint32_t t0 = clockTicks();

		switch (n->task.getId()) { // could use func ptrs, but then all would take args, and all would return vals, this should perform well
			case TaskEncoder: {
				extern void simTimerEncoder(uint32_t ticks);
				int32_t ticks = encoder.run(now);
				//simTimerEncoder(ticks);
				n->task.setWait(ticks);
				tasks.reschedule(n);
				}
				break;
			case TaskEvents: {
				//extern void simTimerEvents(uint32_t ticks);
				int32_t ticks = runEvents(now, 0, 5);
				ticks = tdiff32(ticks, clockTicks());
				ticks = max(ticks, 1);
				//simTimerEvents(ticks);
				n->task.setWait(ticks); //wait <= 2 ? 0 : wait / 2); // force recalcs
				tasks.reschedule(n);
				}
				break;
			case TaskClock:
				runClock(TicksToMicros(tdiff32(now, tasks.start)) / 1000000L);
				break;
			case TaskSchedule:
				runSchedule();
				n->task.setWait(decoder.getTicks() / 2);
				tasks.reschedule(n);
				break;
			case TaskPins:
				readPins();
				break;
			case TaskRefresh:
				runRefresh(now);
				break;
			case TaskTest:
				runTest();
				break;
			case TaskChanges:
				n->task.setWait(runChanges());
				tasks.reschedule(n);
				break;
			case TaskStatus:
				n->task.setWait(runStatus());
				tasks.reschedule(n);
				break;
			case TaskInput:
				runInput();
				break;
		}

		now = clockTicks();
		n->task.calc(tdiff32(now, t0));
	}

	return tasks.head->task.getSleep(now);
}

void runTasks() {
	tasks.setEpoch();

	while (true) {
		uint32_t now = clockTicks();
		int32_t sleep = runTask(now);

		if (sleep > 0) {
			idleSleep(sleep);
			tasks.slept += sleep;
		}
	}
}

void runPerfectTasks() {
	tasks.setEpoch();

	uint32_t last = clockTicks();

	while (true) {
		uint32_t now = clockTicks();

		if (tdiff32(now, last) > 0) {
			int32_t sleep = runTask(last);

			if (false && sleep > 0) {
				last += sleep;
				tasks.slept += sleep;
			} else
				last += 2; // perfect resolution
		}
	}
}
