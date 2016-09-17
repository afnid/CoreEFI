// copyright to me, released under GPL V3

#include "Interrupts.h"
#include "Params.h"
#include "System.h"
#include "Metrics.h"

#include "GPIO.h"
#include "Decoder.h"
#include "Pins.h"

#ifdef ARDUINO
#include <Arduino.h>

#define NOT_AN_INTERRUPT -1

#endif

static const char *PATH = __FILE__;

enum {
	Interrupt2,
	MaxInterrupts,
};

class Interrupt {
	PinId pid;

public:

	uint8_t id;

	inline void init(const GPIO::PinDef *pd) volatile {
#ifdef ARDUINO
		this->id = digitalPinToInterrupt(pd->getId());
#endif
		this->pid = pd->getId();
	}

	uint16_t digitalRead() volatile {
		return gpio.isPinSet(pid);
	}

	void send(Buffer &send) volatile {
		send.p1(F("isr"));
		send.json(F("id"), id);
		send.json(F("pin"), (uint8_t)pid);
		send.p2();
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

	void initInterrupt(uint8_t id, const GPIO::PinDef *pin) volatile {
		interrupts[id].init(pin);
	}

	void send(Buffer &send) volatile {
		for (uint8_t i = 0; i < MaxInterrupts; i++)
			interrupts[i].send(send);
	}
} interrupts;

#ifdef ARDUINO

static void encoder()
{
	volatile Interrupt *i = interrupts.getInterrupt(Interrupt2);
	decoder.run(clockTicks(), i->digitalRead());
}

#endif

uint16_t initInterrupts() {
	//volatile PinInfo *pin = getParamPin(SensorDEC);
	//interrupts.initInterrupt(Interrupt2, pin);

#ifdef ARDUINO
	//volatile Interrupt *i = interrupts.getInterrupt(Interrupt2);
	//attachInterrupt(i->id, encoder, CHANGE);
#endif

	return sizeof(interrupts);
}

void sendInterrupts(Buffer &send) {
	interrupts.send(send);
}
