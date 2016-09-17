// copyright to me, released under GPL V3

#include "Events.h"
#include "Decoder.h"
#include "Broker.h"
#include "Metrics.h"
#include "Params.h"
#include "Schedule.h"
#include "Tasks.h"
#include "GPIO.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

enum {
	CountLate0,
	CountLate1,
	CountLate2,
	CountLate3,
	CountLate4,
	CountLateX,
	CountLates, // 6

	CountEvents,
	CountCalls,
	CountSwaps, // 9

	CountPast,
	CountJumps1,
	CountJumps2, // 12

	CountResets,
	CountSyncs,
	CountReturns, // 15

	CountDelays,
	CountPins,

	HistMax,
};

static const uint8_t MAX_EVENTS = MaxCylinders * 4;
static const uint8_t BucketDivisor = 1;

class BitEvent {
public:

	uint32_t act;
	int32_t late;
	int16_t drift;
	uint8_t calcs;
	uint8_t calls;

	inline void clear() volatile {
		act = 0;
		late = 0;
		drift = 0;
		calcs = 0;
		calls = 0;
	}
};

static volatile struct {
	BitEvent queue[MAX_EVENTS];
	BitSchedule schedule1;
	BitSchedule schedule2;
	volatile BitSchedule *current;
	bool swap;

	uint16_t hist[HistMax];

	int32_t minlate;
	int32_t maxlate;
	uint32_t next;
	int8_t idx;
	int8_t on;
	bool valid;
	bool sync;

	inline void reset() volatile {
		next = 0;
		on = 0;
		idx = 0;

		valid = false;
		sync = false;
		swap = false;

		current = &schedule1;
	}

	inline void addHist(uint8_t h) volatile {
		if (h < HistMax && hist[h] + 1 != 0)
			hist[h]++;
	}

	inline volatile BitEvent *getEvent() volatile {
		if (idx >= current->size()) {
			sync = false;
			on = 0;
			idx = 0;
			checkSwap();
		}

		return queue + idx;
	}

	inline volatile BitPlan *getPlan() volatile {
		return current->getPlan(idx);
	}

	inline void calcNext(uint32_t now, volatile BitEvent *e, volatile BitPlan *p) volatile {
		uint32_t max = decoder.getTicks();
		uint32_t val = p->calcTicks(max, true);

		e->clear();
		e->calcs++;
		p->setMax(max);

		next = decoder.getTDC() + val;
		int32_t diff = tdiff32(next, now);
		int32_t mid = max >> 1;

		if (idx == 0 && diff < -mid) { // expected phase change, computing first event before tdc
			next += max;
			diff = tdiff32(next, now);
		}

		if (diff > mid) { // phasing, can happen with decel only?
			addHist(CountJumps1);
		} else if (diff < -mid) {
			addHist(CountJumps2);
		}

		if (diff < 1)
			addHist(CountPast);
	}

	inline void calcNext(uint32_t now) volatile {
		volatile BitEvent *e = getEvent();
		volatile BitPlan *p = getPlan();
		calcNext(now, e, p);
	}

	inline void checkSwap() volatile {
		if (swap) {
			current = current == &schedule2 ? &schedule1 : &schedule2;
			addHist(CountSwaps);
			swap = false;
		}
	}

	inline uint32_t runEvent(uint32_t now, uint8_t maxdelay, uint16_t jitter) volatile {
		volatile BitEvent *e = getEvent();
		volatile BitPlan *p = getPlan();

		int32_t delay = tdiff32(next, now);

		if (!e->calcs || (!p->hasOff() && delay > jitter)) { // relative events absolute
			calcNext(now, e, p);
			delay = tdiff32(next, now);
		}

		addHist(CountCalls);

		e->calls++;

		if (delay <= jitter) {
			if (delay > 0 && delay <= maxdelay) {
				//delayTicks(delay);
				//now += delay;
				addHist(CountDelays);
			}

			e->late = tdiff32(now, next);
			minlate = min(minlate, e->late);
			maxlate = max(maxlate, e->late);

			e->act = tdiff32(now, decoder.getTDC());
			e->drift = e->act - p->getPos();

			if (!sync)
				addHist(CountSyncs);

			addHist(CountEvents);

			if (e->late) {
				int16_t idx = abs(e->late);

				if (idx >= (TICKTOUS >> 1)) {
					idx /= MicrosToTicks(BucketDivisor);

					addHist(CountLates);

					if (idx < CountLateX)
						addHist(idx);
					else
						addHist(CountLateX);
				}
			}

			GPIO::PinId id = (GPIO::PinId)(p->getPin() + 1);
			bool isSet = GPIO::isPinSet(id);

			if (p->isHi() == isSet)
				addHist(CountPins);

			if (p->isHi()) {
				GPIO::setPin(id, true);
				on++;
			} else {
				GPIO::setPin(id, false);
				on--;
			}

			idx++;

			now = clockTicks();
			calcNext(now);
		}

		return next;
	}
} events;

volatile BitSchedule *getSchedule() {
	if (events.swap)
		return 0;
	return events.current == &events.schedule2 ? &events.schedule1 : &events.schedule2;
}

void swapSchedule() {
	events.swap = true;

	if (!events.current->size())
		events.checkSwap();
}

void setEventTDC(uint32_t tdc) {
	if (events.current->size()) {
		int32_t diff = tdiff32(events.next, tdc);

		if (!events.valid || events.idx != 0 || diff < 0) {
			events.idx = 0;
			events.calcNext(tdc);
			events.addHist(CountResets);
		}

		events.valid = true;
	}

	events.sync = true;
}

uint32_t runEvents(uint32_t now, uint8_t maxdelay, uint16_t jitter) {
	if (events.valid)
		return events.runEvent(now, maxdelay, jitter);

	events.addHist(CountReturns);

	return now + MicrosToTicks(10000);
}

void refreshEvents() {
	if (events.valid && getParamUnsigned(SensorRPM) <= 0)
		events.reset();
}

void sendEventStatus(Buffer &send, BrokerEvent &be, void *data) {
	send.p1(F("events"));

	send.json(F("idx"), events.idx);
	send.json(F("on"), events.on);
	send.json(F("rpm"), getParamUnsigned(SensorRPM));

	float minlate = TicksToMicrosf(events.minlate);
	float maxlate = TicksToMicrosf(events.maxlate);
	send.json(F("-late"), minlate);
	send.json(F("+late"), maxlate);

	uint16_t us = TicksToMicros(decoder.getTicks() >> 1);
	send.json(F("-deg"), us == 0 || events.minlate == 0 ? 0 : 360.0f * minlate / us);
	send.json(F("+deg"), us == 0 || events.maxlate == 0 ? 0 : 360.0f * maxlate / us);

	int16_t total = events.hist[CountEvents];

	if (total && events.valid) {
		int16_t lates = events.hist[CountLates];
		send.json(F("late%"), 100.0f * lates / total);
		uint8_t bucket = BucketDivisor;

		for (uint8_t i = CountLate0; i <= CountLate4; i++) {
			lates -= events.hist[i];

			if (lates)
				send.json(bucket, 100.0f * lates / total);

			bucket += BucketDivisor;
		}
	}

	sendHist(send, events.hist, HistMax);
	events.minlate = 32768;
	events.maxlate = -32767;

	send.p2();
}

void sendEventList(Buffer &send, BrokerEvent &be, void *data) {
	volatile BitSchedule *current = events.current;
	uint32_t last = 0;

	bool monitoring = isParamSet(FlagIsMonitoring);

	for (uint8_t i = 0; i < events.current->size(); i++) {
		volatile BitEvent *e = events.queue + i;
		volatile BitPlan *p = current->getPlan(i);

		send.p1(F("evt"));

		send.json(F("i"), i);
		send.json(F("cyl"), p->getCyl());
		send.json(F("pin"), p->getPin());
		send.json(F("hi"), p->isHi());
		send.json(F("max"), p->getMax());
		send.json(F("pos"), p->getPos());

		if (!monitoring) {
			uint32_t rel = p->getPos() - last;

			send.json(F("rel"), rel);
			send.json(F("id"), p->getId());
			send.json(F("ang"), p->getAng());
			send.json(F("off"), p->getOff());

			send.json(F("calls"), e->calls);
			send.json(F("calcs"), e->calcs);
			send.json(F("drift"), e->drift);
			send.json(F("late"), e->late);
			send.json(F("act"), e->act);
			send.json(F("events"), events.hist[CountEvents]);

			last = p->getPos();
		}

		send.p2();
	}
}

static uint32_t runEvents(uint32_t now, void *data) {
	int32_t ticks = runEvents(now, 0, 5);
	ticks = tdiff32(ticks, clockTicks());
	return max(ticks, 1);
}

uint16_t initEvents() {
	events.reset();

	events.schedule1.reset();
	events.schedule2.reset();

	for (uint8_t i = 0; i < MAX_EVENTS; i++)
		events.queue[i].clear();

	refreshEvents();

	taskmgr.addTask(F("Events"), runEvents, 0, 1000);

	broker.add(sendEventStatus, 0, F("e0"));
	broker.add(sendEventList, 0, F("e1"));

	return sizeof(events);
}
