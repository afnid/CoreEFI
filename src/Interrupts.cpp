// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Pins.h"
#include "Interrupts.h"
#include "Decoder.h"

#ifdef ARDUINO
#include <Arduino.h>

#define NOT_AN_INTERRUPT -1

#endif

enum {
	Interrupt2,
	MaxInterrupts,
};

class Interrupt {
	volatile PinInfo *pin;

public:

	uint8_t id;

	inline void init(volatile PinInfo *pin) volatile {
#ifdef ARDUINO
		this->id = digitalPinToInterrupt(pin->getPin());
#else
		this->id = id;
#endif
		this->pin = pin;
	}

	uint16_t digitalRead() volatile {
		return pin->digitalRead();
	}

	void send() volatile {
		channel.p1(F("isr"));
		channel.send(F("id"), id);
		channel.send(F("pin"), pin, false);
		channel.p2();
		channel.nl();
	}
};

static volatile struct {
private:

	Interrupt interrupts[MaxInterrupts];

public:

	inline volatile Interrupt* getInterrupt(uint8_t id) volatile {
		assert(id >= 0 && id < MaxInterrupts);
		return interrupts + id;
	}

	void initInterrupt(uint8_t id, volatile PinInfo *pin) volatile {
		interrupts[id].init(pin);
	}

	void send() volatile {
		for (uint8_t i = 0; i < MaxInterrupts; i++)
			interrupts[i].send();
	}
} interrupts;

#ifdef ARDUINO

static void encoder()
{
	volatile Interrupt *i = interrupts.getInterrupt(Interrupt2);
	decoder.run(clock_ticks(), i->digitalRead());
}

#endif

uint16_t initInterrupts() {
	volatile PinInfo *pin = getParamPin(SensorDEC);
	interrupts.initInterrupt(Interrupt2, pin);

#ifdef ARDUINO
	//volatile Interrupt *i = interrupts.getInterrupt(Interrupt2);
	//attachInterrupt(i->id, encoder, CHANGE);
#endif

	return sizeof(interrupts);
}

void sendInterrupts() {
	interrupts.send();
}
