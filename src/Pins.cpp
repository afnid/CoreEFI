// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Params.h"

#include "Pins.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifndef digitalPinToPort
#define digitalPinToPort(P) 0
#endif

#ifndef digitalPinToBitMask
#define digitalPinToBitMask(p) (1)
#endif

static volatile class Pins {
	static const uint8_t EXTRA_PINS = 20;
	static const uint8_t MAX_PINS = MaxCylinders + MaxCoils + EXTRA_PINS;

	class PinDetails: public PinInfo {
	public:
		uint16_t val;
		uint8_t mode :4;
		uint8_t hyst :4;

		inline void clear() volatile {
			PinInfo::init(0, 0, 0);

			val = 0;
			mode = 0;
			hyst = 0;
		}
	};

	enum {
		PinUndef = 0,
		PinInput = 1,
		PinOutput = 2,
		PinAnalog = 4,
		PinISR = 8,
	};

	uint8_t valid;
	uint8_t injectors[MaxCylinders];
	uint8_t sparks[MaxCylinders];
	uint8_t params[EXTRA_PINS + 2]; // offset
	PinDetails pins[MAX_PINS];

	inline void clear() volatile {
		valid = 0;

		for (uint8_t i = 0; i < MAX_PINS; i++)
			pins[i].clear();

		for (uint8_t i = 0; i < sizeof(injectors); i++)
			injectors[i] = 0;
		for (uint8_t i = 0; i < sizeof(sparks); i++)
			sparks[i] = 0;
		for (uint8_t i = 0; i < sizeof(params); i++)
			params[i] = 0;
	}

	uint8_t initPin(uint8_t pin, uint8_t mode, uint8_t hyst) volatile {
		assert(valid >= 0 && valid < MAX_PINS);
		volatile PinDetails *p = pins + valid;
		assert(!p->getMask());
		assert(!p->getPin());

		volatile uint8_t *port = 0;
		uint8_t mask = 0;

#ifndef ARDUINO_DUE
		port = digitalPinToPort(pin);
		mask = 1 << digitalPinToBitMask(pin);
#endif

		if (mask == 0)
			mask = -1;

		p->init(pin, mask, port);
		p->mode = mode;
		p->hyst = hyst;

		assert(p->getPin() == pin);
		assert(p->hyst == hyst);

		valid++;

#ifdef ARDUINO
		if (mode & (PinInput|PinISR))
			pinMode(pin, INPUT);
		if (mode & PinOutput)
			pinMode(pin, OUTPUT);
#endif
		return valid - 1;
	}

	void initParamPin(uint8_t pin, uint8_t id, uint8_t mode, uint8_t hyst) volatile {
		assert(id >= 0 && id < sizeof(params));
		params[id] = initPin(pin, mode, hyst);
	}

	void initInjectorPin(uint8_t pin, uint8_t cyl) volatile {
		assert(cyl >= 0 && cyl < sizeof(injectors));
		injectors[cyl] = initPin(pin, PinOutput, 0);
	}

	void initSparkPin(uint8_t pin, uint8_t cyl, uint8_t coils) volatile {
		assert(cyl >= 0 && cyl < sizeof(sparks));

		if (cyl >= coils) {
			sparks[cyl] = sparks[0];
			return;
		}

		//coils == 1 ? cylinders : cyl + cylinders * 2;
		sparks[cyl] = initPin(pin, PinOutput, 0);
	}

public:

	inline void initPins() volatile {
		clear();

		uint8_t cyl = getParamUnsigned(ConstCylinders);
		uint8_t engine = 2;

		for (uint8_t i = 0; i < cyl; i++)
			initInjectorPin(engine++, i);

		uint8_t coils = getParamUnsigned(ConstCoils);

		for (uint8_t i = 0; i < cyl; i++)
			initSparkPin(engine++, i, coils);

		uint8_t analog = 8;
		initParamPin(analog++, SensorMAF, PinAnalog, 1);
		initParamPin(analog++, SensorACT, PinAnalog, 1);
		initParamPin(analog++, SensorECT, PinAnalog, 1);
		initParamPin(analog++, SensorHEGO1, PinAnalog, 1);
		initParamPin(analog++, SensorHEGO2, PinAnalog, 1);
		initParamPin(analog++, SensorBAR, PinAnalog, 1);
		initParamPin(analog++, SensorEGR, PinAnalog, 1);
		initParamPin(analog++, SensorVCC, PinAnalog, 1);

		uint8_t digital = 21; // interrupt 2
		initParamPin(digital++, SensorDEC, PinISR, 0);
		initParamPin(digital++, SensorTPS, PinInput, 0);
		initParamPin(digital++, SensorVSS, PinInput, 0);
		initParamPin(digital++, SensorGEAR, PinInput, 0);
		initParamPin(digital++, SensorIsKeyOn, PinInput, 0);
		initParamPin(digital++, SensorIsCranking, PinInput, 0);
		initParamPin(digital++, SensorIsAirOn, PinInput, 0);
		initParamPin(digital++, SensorIsBrakeOn, PinInput, 0);

		initParamPin(digital++, CalcFuelPump, PinOutput, 0);
		initParamPin(digital++, CalcFan1, PinOutput, 0);
		initParamPin(digital++, CalcFan2, PinOutput, 0);
		initParamPin(digital++, CalcEPAS, PinOutput, 0);
	}

	inline volatile PinDetails *getPin(uint8_t idx) volatile {
		assert(idx < MAX_PINS);
		return pins + idx;
	}

	inline volatile PinInfo *getParamPin(uint8_t id) volatile {
		assert(id < sizeof(params));
		int idx = params[id];
		return getPin(idx);
	}

	inline uint8_t getSparkPin(uint8_t cyl) volatile {
		int max = getParamUnsigned(ConstCoils);

		while (cyl && cyl >= max)
			cyl >>= 1;

		assert(cyl < sizeof(sparks));
		return sparks[cyl];
	}

	inline uint8_t getInjectorPin(uint8_t cyl) volatile {
		int max = getParamUnsigned(ConstCylinders);

		while (cyl && cyl >= max)
			cyl >>= 1;

		assert(cyl < sizeof(injectors));
		return injectors[cyl];
	}

	inline void resetOutputPins() volatile {
		for (uint8_t id = 0; id < sizeof(params); id++) {
			if (params[id]) {
				volatile PinDetails *p = getPin(id);

				if (p->mode == PinOutput)
					p->clr();
			}
		}
	}

	inline void readPins() volatile {
		for (uint8_t id = 0; id < sizeof(params); id++) {
			if (params[id]) {
				volatile PinDetails *p = getPin(id);

				if (p->mode == PinAnalog) {
					uint16_t val = p->analogRead();

					if (abs(val - p->val) > p->hyst) {
						p->val = val;
						setSensorParam(id, val);
					}
				} else if (p->mode == PinInput) {
					uint8_t val = p->digitalRead();

					if (val != p->val) {
						p->val = val;
						setSensorParam(id, val);
					}
				}
			}
		}
	}

	inline void sendPins() volatile {
		channel.p1(F("pins"));
		channel.send(F("valid"), valid);
		channel.send(F("data"), (uint16_t) sizeof(Pins));
		channel.send(F("max"), MAX_PINS);
		channel.send(F("used"), valid);
		channel.p2();
		channel.nl();

		for (uint8_t i = 0; i < MAX_PINS; i++) {
			volatile PinDetails *p = pins + i;

			if (p->mode) {
				channel.p1(F("pin"));
				channel.send(F("i"), i);
				channel.send(F("pin"), p->getPin());
				channel.send(F("mode"), p->mode);
				channel.send(F("val"), p->val);
				channel.send(F("hyst"), p->hyst);
				channel.send(F("mask"), p->getMask());
				channel.send(F("port"), p->getPort() != 0);
				channel.p2();
				channel.nl();
			}
		}
	}
} local;

volatile PinInfo *getPin(uint8_t id) {
	return local.getPin(id);
}

volatile PinInfo *getParamPin(uint8_t id) {
	return local.getParamPin(id);
}

uint8_t getSparkPin(uint8_t cyl) {
	return local.getSparkPin(cyl);
}

uint8_t getInjectorPin(uint8_t cyl) {
	return local.getInjectorPin(cyl);
}

uint16_t initPins() {
	local.initPins();
	return sizeof(local);
}

void resetOutputPins() {
	local.resetOutputPins();
}

void readPins() {
	local.readPins();
}

void sendPins() {
	local.sendPins();
}

void PinInfo::digitalWrite(uint8_t v) const volatile {
#ifdef ARDUINO
	return ::digitalWrite(pin, v);
#else
#endif
}

uint16_t PinInfo::digitalRead() const volatile {
#ifdef ARDUINO
	return ::digitalRead(pin);
#else
	return pin;
#endif
}

uint16_t PinInfo::analogRead() const volatile {
#ifdef ARDUINO
	return ::analogRead(pin);
#else
	return pin;
#endif
}
