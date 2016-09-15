// copyright to me, released under GPL V3

#ifndef _Events_h_
#define _Events_h_

#include "utils.h"

class BitPlan {
protected:

	uint32_t pos;
	uint32_t max;

	uint16_t id :6;
	uint16_t hi :1;
	uint16_t cyl :4;
	uint16_t pin :5;

	uint16_t ang;
	int16_t off;

public:

	inline void setEvent(uint8_t id, uint8_t cyl, uint8_t pin, uint16_t ang, int16_t off, bool hi) volatile {
		this->id = id;
		this->pin = pin;
		this->cyl = cyl;
		this->hi = hi;
		this->ang = ang;
		this->off = off;
		this->pos = 0;
		assert(this->pin == pin);
		assert(this->cyl == cyl);
		assert(this->id == id);
	}

	inline void setEvent(const BitPlan *e) volatile {
		id = e->getId();
		cyl = e->getCyl();
		pin = e->getPin();
		ang = e->getAng();
		off = e->getOff();
		pos = e->getPos();
		hi = e->isHi();
	}

	inline uint32_t getPos() const volatile {
		return pos;
	}

	inline uint32_t getMax() const volatile {
		return max;
	}

	inline uint16_t getAng() const volatile {
		return ang;
	}

	inline int16_t getOff() const volatile {
		return off;
	}

	inline uint8_t getId() const volatile {
		return id;
	}

	inline uint8_t getCyl() const volatile {
		return cyl;
	}

	inline uint8_t getPin() const volatile {
		return pin;
	}

	inline bool isHi() const volatile {
		return hi;
	}

	inline bool hasOff() const volatile {
		return off != 0;
	}

	inline void setMax(uint32_t max) volatile {
		this->max = max;
		assert(this->max == max);
	}

	inline void calcPos(uint32_t max) volatile {
		uint32_t pos = calcTicks(max);
		this->max = max;
		this->pos = pos;
		assert(this->max == max);
		assert(this->pos == pos);
	}

	inline uint32_t calcTicks(uint32_t max, bool wrap = true) const volatile {
		uint32_t ticks = max;

		if (max > 65535u) {
			int shift = 1;
			ticks >>= 1;

			while (ticks > 65535u) {
				shift++;
				ticks >>= 1;
			}

			ticks = ((ticks * ang) >> (16 - shift));
		} else
			ticks = ((ticks * ang) >> 16);

		if (off) {
			uint16_t v = abs(off);

			if (off > 0 && v > max)
				ticks += max - 1;
			else if (off < 0 && v > max)
				ticks += 1;
			else if (off < 0 && v > ticks)
				ticks += max + off;
			else
				ticks += off;

			if (wrap && ticks > max)
				ticks -= max;
		}

		return ticks;
	}
};

uint16_t initEvents();
void refreshEvents();
uint32_t runEvents(uint32_t now, uint8_t maxdelay, uint16_t jitter);
void checkEvents();
void sendEventStatus(void *data);
void sendEventList(void *data);

#endif
