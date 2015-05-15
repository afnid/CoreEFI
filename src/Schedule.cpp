// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Events.h"
#include "Decoder.h"
#include "Schedule.h"
#include "Pins.h"

class Schedule {
	static const double DIV720 = 65536 / 720.0;

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
		initMetric(MetricSparkAdvance);
		uint16_t sparkAdvance = (uint16_t)(DIV720 * getParamDouble(CalcFinalSparkAdvance));
		calcMetric(MetricSparkAdvance);

		if (this->sparkAdvance != sparkAdvance) {
			this->sparkAdvance = sparkAdvance;
			changed = true;
		}
	}

	inline void calcPulseAdvance() {
		initMetric(MetricPulseAdvance);
		uint16_t pulseAdvance = (uint16_t)(DIV720 * getParamDouble(CalcFinalPulseAdvance));
		calcMetric(MetricPulseAdvance);

		if (this->pulseAdvance != pulseAdvance) {
			this->pulseAdvance = pulseAdvance;
			changed = true;
		}
	}

	inline void calcPulseWidth() {
		initMetric(MetricPulseWidth);
		uint16_t pulseWidth = getParamDouble(CalcFinalPulseWidth);
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
			double arc = 65536.0 / cylinders;
			uint8_t n = 0;

			volatile BitPlan *p = schedule->getPlans();

			for (uint8_t cyl = 0; cyl < cylinders; cyl++) {
				uint16_t tdc = (uint16_t) (arc * cyl);
				uint16_t pulse1 = tdc - pulseAdvance;
				uint16_t spark1 = tdc - sparkAdvance;

				uint8_t pulsepin = getInjectorPin(cyl);
				uint8_t sparkpin = getSparkPin(cyl);

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

	inline void sendSchedule() {
		channel.p1(F("schedule"));
		channel.send(F("sparkAdvance"), sparkAdvance);
		channel.send(F("pulseAdvance"), pulseAdvance);
		channel.send(F("pulseWidth"), pulseWidth);
		channel.send(F("sparkDuration"), sparkDuration);
		Metric::send(metrics, MetricMax);
		sendHist(hist, HistMax);

		//channel.p1(F("index"));
		//for (uint8_t i = 0; i < EVENTS; i++)
		//channel.send(i, (uint16_t)schedule.index[i]);
		//channel.p2();

		channel.p2();
		channel.nl();
	}
} schedule;

void runSchedule() {
	schedule.runSchedule();
}

uint16_t initSchedule() {
	myzero(&schedule, sizeof(schedule));
	return sizeof(schedule);
}

void sendSchedule() {
	schedule.sendSchedule();
}
