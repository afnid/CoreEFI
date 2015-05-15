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
	CountDelays,
	CountSwaps,

	CountResets,
	CountSyncs,
	CountJumps, // 12
	CountPast,

	CountReturns,
	CountDrift,
	CountPins,

	HistMax,
};

static const uint8_t MAX_EVENTS = MaxCylinders * 4;

class BitEvent {
public:

	uint32_t act;
	int16_t late;
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
	uint32_t tdc;
	uint32_t next;
	uint32_t last;
	int8_t idx;
	int8_t on;
	bool valid;
	bool sync;

	inline void reset() volatile {
		next = 0;
		last = 0;
		on = 0;
		idx = 0;

		current = &schedule1;

		valid = false;
		sync = false;
	}

	inline void addHist(uint8_t h) volatile {
		if (h < HistMax && hist[h] + 1 != 0)
			hist[h]++;
	}

	inline volatile BitEvent *getEvent() volatile {
		if (idx >= current->size()) {
			sync = false;
			idx = 0;
			checkSwap();
		}

		return queue + idx;
	}

	inline volatile BitPlan *getPlan() volatile {
		return current->getPlan(idx);
	}

	inline void calcNext(uint32_t now, volatile BitEvent *e, volatile BitPlan *p) volatile {
		uint32_t tdc = decoder.getTDC();
		uint32_t max = decoder.getTicks();
		uint32_t val = p->calcTicks(max, true);

		int32_t last = next;
		next = tdc + val;

		if (idx == 0 && tdiff32(next, last) < 0)
			next += max;

		e->calcs++;
		p->setMax(max);

		last = tdiff32(next, last);

		if (last < 0)
			addHist(CountPast);
		else if ((uint32_t)last > max / 2) {
			//uint32_t ticks = clock_ticks();
			//printf("%d next=%d tdc=%d last=%d max=%u\n", idx, tdiff32(ticks, next), tdiff32(ticks, tdc), last, max);
			addHist(CountJumps);
		}
	}

	inline void checkSwap() volatile {
		if (swap && idx == 0) {
			current = current == &schedule2 ? &schedule1 : &schedule2;
			addHist(CountSwaps);
			swap = false;
		}
	}

	inline uint32_t runEvent(uint32_t now, uint8_t maxdelay) volatile {
		volatile BitEvent *e = getEvent();
		volatile BitPlan *p = getPlan();

		int32_t delay = tdiff32(next, now);

		if (!e->calcs || (!p->hasOff() && delay > maxdelay)) { // relative events absolute
			calcNext(now, e, p);
			delay = tdiff32(next, now);
		}

		e->calls++;

		if (delay <= maxdelay) {
			if (delay > 0) {
				delay_ticks(delay);
				now += delay;
				addHist(CountDelays);
			}

			e->late = tdiff32(now, next);
			minlate = min(minlate, e->late);
			maxlate = max(maxlate, e->late);

			e->act = tdiff32(now, tdc);
			e->drift = e->act - p->getPos();

			if (!sync)
				addHist(CountSyncs);
			//if (e->drift)
				//addHist(CountDrift);

			addHist(CountEvents);

			if (e->late > 0) {
				int16_t abserr = abs(e->late) / 4;
				addHist(CountLates);

				if (abserr < CountLateX)
					addHist(abserr);
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

			e = getEvent();
			p = getPlan();
			e->clear();

			calcNext(now, e, p);
			now = clock_ticks();
			delay = tdiff32(next, now);
		}

		last = now;

		return delay > 0 ? delay - 1 : 0;
	}
} events;

volatile BitSchedule *getSchedule()
{
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
	events.on = 0;

	if (events.valid && events.idx != 0) {
#ifndef ARDUINO
		//sendEventList();
#endif
		events.idx = 0;
		events.addHist(CountResets);
	}

	events.valid = true;
	events.sync = true;
	events.tdc = tdc;
}

uint32_t runEvents(uint32_t now, uint8_t jitter) {
	if (decoder.isValid() && events.current->size())
		return events.runEvent(now, jitter);

	events.addHist(CountReturns);

	return 65535;
}

void refreshEvents() {
	if (decoder.isValid() && getParamUnsigned(SensorRPM) <= 0)
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

	channel.send(F("on"), events.on);
	channel.send(F("-late"), events.minlate, false);
	channel.send(F("+late"), events.maxlate);

	channel.send(F("rpm"), getParamUnsigned(SensorRPM));
	uint16_t us = TicksToMicros(decoder.getTicks() >> 1);
	channel.send(F("-deg"), us == 0 || events.minlate == 0 ? 0 : 360.0 * events.minlate / us, false);
	channel.send(F("+deg"), us == 0 || events.maxlate == 0 ? 0 : 360.0 * events.maxlate / us);

	int16_t total = events.hist[CountEvents];

	if (total && decoder.isValid()) {
		int16_t lates = events.hist[CountLates];
		channel.send(F("late%"), 100.0 * lates / total);

		for (uint8_t i = CountLate0; i <= CountLate0; i++) {
			lates -= events.hist[i];
			channel.send(F("latex"), 100.0 * lates / total);
		}
	}

	sendHist(events.hist, HistMax);
	events.minlate = 30000;
	events.maxlate = -events.minlate;

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
