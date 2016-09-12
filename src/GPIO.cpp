#include "GPIO.h"

#ifndef NDEBUG
#define MYNAME(x)	x, #x
#else
#define MYNAME(mode, x, pin)	x, 0, 0
#endif

#include "efi_pins.h"

const GPIO::PinDef *GPIO::getPinDef(PinId id) {
	return pins + id;
}

bool GPIO::isPinSet(PinId id) {
#ifdef ARDUINO
	uint16_t last = ::digitalRead(pins[id].pin);
#else
	uint16_t last = pins[id].last;

	if (pins[id].last != last) {
		pins[id].last = last;
		pins[id].changed = millis();
	}

	return pins[id].last;
}

void GPIO::setPin(PinId id, bool v) {
	if (pins[id].last != v) {
		pins[id].last = v;
		pins[id].changed = millis();
	}

#ifdef ARDUINO
	::digitalWrite(pins[id].pin, v);
#endif
}

uint16_t GPIO::analogRead(PinId id) {
#ifdef ARDUINO
	uint16_t last = ::analogRead(pins[id].pin);
#else
	uint16_t last = pins[id].last;
#endif

	if (pins[id].last != last) {
		pins[id].last = last;
		pins[id].changed = millis();
	}

	return pins[id].last;
}

void GPIO::analogWrite(PinId id, uint16_t v) {
	if (pins[id].last != v) {
		pins[id].last = v;
		pins[id].changed = millis();
	}

#ifdef ARDUINO
	::analogWrite(pins[id].pin, v);
#endif
}

void GPIO::init() {
	for (uint8_t i = 0; i < MaxPins; i++) {
		PinId id = (PinId)i;
		const PinDef *pd = getPinDef(id);

#ifdef ARDUINO
		switch (pd->type) {
			case PinModeOutput:
			case PinModeOutputPWM:
				pinMode(def.pin, OUTPUT);
				break;
			case PinModeInput:
				pinMode(def.pin, INPUT_PULLUP);
				break;
			case PinModeInputPullup:
				pinMode(def.pin, INPUT_PULLUP);
				break;
		}
#endif
	}
}

#endif

