// copyright to me, released under GPL V3

#include "Schedule.h"

#include "Encoder.h"
#include "Metrics.h"
#include "Broker.h"
#include "Tasks.h"
#include "Hardware.h"
#include "GPIO.h"

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
		volatile BitSchedule *schedule = BitSchedule::getSchedule();

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

				uint8_t pulsepin = PinInjector1 + cyl;
				uint8_t sparkpin = PinSpark1 + cyl;

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

			BitSchedule::swapSchedule();

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
		MetricsHist::sendHist(send, F("counts"), hist, HistMax);

		//send.p1(F("index"));
		//for (uint8_t i = 0; i < EVENTS; i++)
		//send.json(i, (uint16_t)schedule.index[i]);

		send.p2();
	}
} schedule;

static inline uint32_t runStatus(uint32_t t0, void *data) {
	uint32_t waitms = getParamUnsigned(FlagIsMonitoring);

	toggleled(0);

	Buffer &send = hardware.send();

	if (!waitms) {
		BitPlan::sendEventStatus(send);
		return 3000 * 1000UL;
	}

	decoder.sendList(send);
	BitPlan::sendEventList(send);

	waitms = max(waitms, 500);

	return waitms * 1000UL;
}

static inline uint32_t runRefresh(uint32_t now, void *data) {
	BitPlan::refreshEvents();
	encoder.refresh();
	decoder.refresh(now);
	return 0;
}

uint32_t BitSchedule::runSchedule(uint32_t t0, void *data) {
	schedule.runSchedule();
	return decoder.getTicks() / 2;
}

static void brokercb(Buffer &send, BrokerEvent &be, void *data) {
	schedule.sendSchedule(send);
}

void BitSchedule::init() {
	bzero(&schedule, sizeof(schedule));

	taskmgr.addTask(F("Schedule"), runSchedule, 0, 25);
	taskmgr.addTask(F("Refresh"), runRefresh, 0, 2000);
	taskmgr.addTask(F("Status"), runStatus, 0, 3000);

	broker.add(brokercb, 0, F("s"));
}

uint16_t BitSchedule::mem(bool alloced) {
	return sizeof(schedule);
}
