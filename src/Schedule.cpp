// copyright to me, released under GPL V3

#include "System.h"
#include "Buffer.h"
#include "Prompt.h"
#include "Metrics.h"
#include "Params.h"

#include "Tasks.h"
#include "Events.h"
#include "Decoder.h"
#include "Encoder.h"
#include "Schedule.h"
#include "Pins.h"

class Schedule {
	enum {
		MetricSparkAdvance,
		MetricPulseAdvance,
		MetricPulseWidth,
		MetricSparkWidth,

		MetricEvents,
		MetricOrder,
		MetricMerge,

		MetricMax
	};

	enum {
		CountChanges,
		CountSorts,
		HistMax
	};

	Metric metrics[MetricMax];
	uint16_t hist[HistMax];
	uint8_t step;
	bool changed;

	uint16_t sparkAdvance;
	uint16_t pulseAdvance;
	uint16_t pulseWidth;
	uint16_t sparkDuration;

	inline void initMetric(uint8_t id) {
		if (id < MetricMax)
			metrics[id].init();
	}

	inline void calcMetric(uint8_t id) {
		if (id < MetricMax)
			metrics[id].calc();
	}

	inline void addHist(uint8_t h) {
		if (h < HistMax && hist[h] + 1 != 0)
			hist[h]++;
	}

	inline void calcSparkAdvance() {
		static const float DIV720 = 65536 / 720.0;
		initMetric(MetricSparkAdvance);
		uint16_t sparkAdvance = (uint16_t)(DIV720 * getParamFloat(CalcFinalSparkAdvance));
		calcMetric(MetricSparkAdvance);

		if (this->sparkAdvance != sparkAdvance) {
			this->sparkAdvance = sparkAdvance;
			changed = true;
		}
	}

	inline void calcPulseAdvance() {
		static const float DIV720 = 65536 / 720.0;
		initMetric(MetricPulseAdvance);
		uint16_t pulseAdvance = (uint16_t)(DIV720 * getParamFloat(CalcFinalPulseAdvance));
		calcMetric(MetricPulseAdvance);

		if (this->pulseAdvance != pulseAdvance) {
			this->pulseAdvance = pulseAdvance;
			changed = true;
		}
	}

	inline void calcPulseWidth() {
		initMetric(MetricPulseWidth);
		uint16_t pulseWidth = getParamFloat(CalcFinalPulseWidth);
		calcMetric(MetricPulseWidth);

		if (this->pulseWidth != pulseWidth) {
			this->pulseWidth = pulseWidth;
			changed = true;
		}
	}

	inline void calcSparkDuration() {
		initMetric(MetricSparkWidth);
		uint16_t sparkDuration = getParamUnsigned(CalcFinalSparkWidth);
		calcMetric(MetricSparkWidth);

		if (this->sparkDuration != sparkDuration) {
			this->sparkDuration = sparkDuration;
			changed = true;
		}
	}

	void schedule() {
		volatile BitSchedule *schedule = getSchedule();

		if (schedule) {
			initMetric(MetricEvents);

			uint8_t cylinders = getParamUnsigned(ConstCylinders);
			float arc = 65536.0 / cylinders;
			uint8_t n = 0;

			volatile BitPlan *p = schedule->getPlans();

			for (uint8_t cyl = 0; cyl < cylinders; cyl++) {
				uint16_t tdc = (uint16_t) (arc * cyl);
				uint16_t pulse1 = tdc - pulseAdvance;
				uint16_t spark1 = tdc - sparkAdvance;

				uint8_t pulsepin = PinMgr::getInjectorPin(cyl);
				uint8_t sparkpin = PinMgr::getSparkPin(cyl);

				p++->setEvent(n++, cyl, pulsepin, pulse1, 0, true);
				p++->setEvent(n++, cyl, pulsepin, pulse1, pulseWidth, false);

				p++->setEvent(n++, cyl, sparkpin, spark1, 0, false);
				p++->setEvent(n++, cyl, sparkpin, spark1, -sparkDuration, true);
			}

			calcMetric(MetricEvents);
			initMetric(MetricOrder);

			if (!schedule->isOrdered(n))
				addHist(CountSorts);

			calcMetric(MetricOrder);
			initMetric(MetricMerge);

			swapSchedule();

			calcMetric(MetricMerge);
		}
	}

public:

	inline void runSchedule() {
		if (getParamUnsigned(SensorRPM) > 0) {
			switch (step++) {
				case 0:
					clearParamCache();
					calcSparkAdvance();
					calcPulseAdvance();
					calcPulseWidth();
					calcSparkDuration();
					break;
				//case 1:
					//calcPulseAdvance();
					//break;
				//case 2:
					//calcPulseWidth();
					//break;
				//case 3:
					//calcSparkDuration();
					//break;
				default:
					changed = true;

					if (changed)
						addHist(CountChanges);

					if (changed && pulseWidth != 0 && sparkDuration != 0) {
						schedule();
						changed = false;
					}

					step = 0;
					break;
			}
		}
	}

	inline void sendSchedule(Buffer &send) {
		send.p1(F("schedule"));
		send.json(F("sparkAdvance"), sparkAdvance);
		send.json(F("pulseAdvance"), pulseAdvance);
		send.json(F("pulseWidth"), pulseWidth);
		send.json(F("sparkDuration"), sparkDuration);
		Metric::send(send, metrics, MetricMax);
		sendHist(send, hist, HistMax);

		//send.p1(F("index"));
		//for (uint8_t i = 0; i < EVENTS; i++)
		//send.json(i, (uint16_t)schedule.index[i]);

		send.p2();
	}
} schedule;

static inline uint32_t runSchedule(uint32_t t0, void *data) {
	schedule.runSchedule();
	return decoder.getTicks() / 2;
}

static inline uint32_t runStatus(uint32_t t0, void *data) {
	uint32_t wait = getParamUnsigned(FlagIsMonitoring);

	toggleled(0);

	if (!wait) {
		sendEventStatus(0);
		return MicrosToTicks(3000017UL);
	}

	extern Buffer channel;
	decoder.sendList(channel);
	sendEventList(0);

	wait = max(wait, 500);

	return wait * 3000;
}

static inline uint32_t runRefresh(uint32_t now, void *data) {
	refreshEvents();
	encoder.refresh();
	decoder.refresh(now);
	return 0;
}

static void sendSchedule(Buffer &send, void *data) {
	schedule.sendSchedule(send);
}

uint16_t initSchedule() {
	myzero(&schedule, sizeof(schedule));

	TaskMgr::addTask(F("Schedule"), runSchedule, 0, 25);
	TaskMgr::addTask(F("Refresh"), runRefresh, 0, 2000);
	TaskMgr::addTask(F("Status"), runStatus, 0, 3000);

	static PromptCallback callbacks[] = {
		{ F("s"), sendSchedule },
	};

	addPromptCallbacks(callbacks, ARRSIZE(callbacks));

	return sizeof(schedule) + sizeof(callbacks);
}
