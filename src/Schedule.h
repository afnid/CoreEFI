// copyright to me, released under GPL V3

#ifndef _Schedule_h_
#define _Schedule_h_

#include "Params.h"
#include "Events.h"
#include "Decoder.h"

class BitSchedule {
	static const uint8_t MAX_EVENTS = MaxCylinders * 4;

	BitPlan plan[MAX_EVENTS];
	uint8_t index[MAX_EVENTS];
	uint8_t count;

	inline void reorder() volatile {
		uint8_t bits[bitsize(MaxCylinders * 4)];
		bzero(bits, bitsize(MaxCylinders * 4));

		int16_t min = 0;
		int16_t max = count - 1;

		for (uint8_t j = 0; j < count; j++) {
			int idx = -1;

			for (int i = min; i <= max; i++)
				if (!isset(bits, i))
					if (idx < 0 || plan[i].getPos() < plan[idx].getPos())
						idx = i;

			if (idx >= 0 && idx < count) {
				bitset(bits, idx);
				index[j] = idx;
			}

			while (min < max && isset(bits, min))
				min++;
			while (min < max && isset(bits, max))
				max--;
		}
	}

public:

	inline bool isOrdered(int count) volatile {
		this->count = count;

		uint32_t max = decoder.getTicks();
		bool ordered = true;
		volatile BitPlan *last = 0;

		for (uint8_t i = 0; i < count; i++) {
			volatile BitPlan *p = plan + index[i];
			p->calcPos(max);

			if (last && last->getPos() > p->getPos())
				ordered = false;

			last = p;
		}

		if (!ordered)
			reorder();

		return ordered;
	}

	inline void reset() volatile {
		BitPlan bp;
		bzero(&bp, sizeof(bp));

		for (uint8_t i = 0; i < MAX_EVENTS; i++) {
			index[i] = i;
			plan[i].setEvent(&bp);
		}
	}

	inline volatile BitPlan *getPlan(int idx) volatile {
		return plan + index[idx];
	}

	inline volatile BitPlan *getPlans() volatile {
		return plan;
	}

	inline uint8_t size() volatile {
		return count;
	}
};

volatile BitSchedule *getSchedule();
void swapSchedule();

uint16_t initSchedule();
void runSchedule();
void sendSchedule();

#endif
