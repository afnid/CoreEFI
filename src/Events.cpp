// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Decoder.h"
#include "Events.h"
#include "Pins.h"
#include "Schedule.h"

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

	inline uint32_t runEvent(uint32_t now, uint8_t maxdelay, uint8_t jitter) volatile {
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
				delayTicks(delay);
				now += delay;
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

			if (e->late > 0) {
				int16_t idx = abs(e->late) / 4;
				addHist(CountLates);

				if (idx < CountLateX)
					addHist(idx);
				else
					addHist(CountLateX);
			}

			volatile PinInfo *pin = getPin(p->getPin());

			if (p->isHi() == pin->isSet())
				addHist(CountPins);

			if (p->isHi()) {
				pin->set();
				on++;
			} else {
				pin->clr();
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

uint32_t runEvents(uint32_t now, uint8_t maxdelay, uint8_t jitter) {
	if (events.valid)
		return events.runEvent(now, maxdelay, jitter);

	events.addHist(CountReturns);

	return now + MicrosToTicks(10000);
}

void refreshEvents() {
	if (events.valid && getParamUnsigned(SensorRPM) <= 0)
		events.reset();
}

uint16_t initEvents() {
	events.reset();

	events.schedule1.reset();
	events.schedule2.reset();

	for (uint8_t i = 0; i < MAX_EVENTS; i++)
		events.queue[i].clear();

	refreshEvents();

	return sizeof(events);
}

void sendEventStatus() {
	channel.p1(F("events"));

	channel.send(F("idx"), events.idx);
	channel.send(F("on"), events.on);
	channel.send(F("rpm"), getParamUnsigned(SensorRPM));

	channel.send(F("-late"), events.minlate, false);
	channel.send(F("+late"), events.maxlate);

	uint16_t us = TicksToMicros(decoder.getTicks() >> 1);
	channel.send(F("-deg"), us == 0 || events.minlate == 0 ? 0 : 360.0f * events.minlate / us, false);
	channel.send(F("+deg"), us == 0 || events.maxlate == 0 ? 0 : 360.0f * events.maxlate / us);

	int16_t total = events.hist[CountEvents];

	if (total && events.valid) {
		int16_t lates = events.hist[CountLates];
		channel.send(F("late%"), 100.0f * lates / total);

		for (uint8_t i = CountLate0; i <= CountLate0; i++) {
			lates -= events.hist[i];
			channel.send(F("latex"), 100.0f * lates / total);
		}
	}

	sendHist(events.hist, HistMax);
	events.minlate = 32768;
	events.maxlate = -32767;

	channel.p2();
	channel.nl();
}

void sendEventList() {
	volatile BitSchedule *current = events.current;
	uint32_t last = 0;

	bool monitoring = isParamSet(FlagIsMonitoring);

	for (uint8_t i = 0; i < events.current->size(); i++) {
		volatile BitEvent *e = events.queue + i;
		volatile BitPlan *p = current->getPlan(i);

		channel.p1(F("evt"));

		channel.send(F("i"), i);
		channel.send(F("cyl"), p->getCyl());
		channel.send(F("pin"), p->getPin());
		channel.send(F("hi"), p->isHi());
		channel.send(F("max"), p->getMax());
		channel.send(F("pos"), p->getPos());

		if (!monitoring) {
			uint32_t rel = p->getPos() - last;

			channel.send(F("rel"), rel);
			channel.send(F("id"), p->getId());
			channel.send(F("ang"), p->getAng());
			channel.send(F("off"), p->getOff());

			channel.send(F("calls"), e->calls);
			channel.send(F("calcs"), e->calcs);
			channel.send(F("drift"), e->drift);
			channel.send(F("late"), e->late);
			channel.send(F("act"), e->act);
			channel.send(F("events"), events.hist[CountEvents]);

			last = p->getPos();
		}

		channel.p2();
		channel.nl();
	}
}
