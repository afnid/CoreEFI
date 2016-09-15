// copyright to me, released under GPL V3

#include "GPIO.h"
#include "Pins.h"

#if 0
#include "System.h"
#include "Buffer.h"
#include "Prompt.h"
#include "Params.h"

#include "Pins.h"
#include "Epoch.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifndef digitalPinToPort
#define digitalPinToPort(P) 0
#endif

#ifndef digitalPinToBitMask
#define digitalPinToBitMask(p) (1)
#endif

class PinInternal: public PinInfo {
	uint8_t *getPinPort(uint8_t pin) {
#if 0 //ndef ARDUINO_DUE
		port = digitalPinToPort(pin);
		mask = 1 << digitalPinToBitMask(pin);
#endif
		return 0;
	}

	uint8_t getPinMask(uint8_t pin) {
		return 0;
	}

public:

	inline bool readPin() {
		if (def->mode & (PinModeInput|PinModeAnalog)) {
			uint16_t v = Device::analogRead(def->id);

			if (abs(v - def->last) > def->hyst) {
				setParamUnsigned(def->id, v);
				def->changed = Epoch::millis();
				def->last = v;
				return true;
			}
		} else if (def->mode & PinModeInput) {
			uint8_t v = Device::digitalRead(def->id);

			if (def->invert)
				v = !v;

			if (def->last != v) {
				setParamUnsigned(def.id, v);
				def->changed = Epoch::millis();
				def->last = v;
				return true;
			}
		}

		return false;
	}
};

static class Local {
public:

	static const uint8_t EXTRA_PINS = 60;
	static const uint8_t MAX_PINS = MaxCylinders + MaxCoils + EXTRA_PINS;

	uint8_t input;
	uint8_t output;
	uint8_t valid;
	uint8_t injectors[MaxCylinders];
	uint8_t sparks[MaxCylinders];
	uint8_t params[EXTRA_PINS + 2]; // offset
	PinInfo *last[4];

	inline void clear() {
		valid = 0;
		input = 0;
		output = 0;

		for (uint8_t i = 0; i < ARRSIZE(last); i++)
			last[i] = 0;

		for (uint8_t i = 0; i < sizeof(injectors); i++)
			injectors[i] = 0;
		for (uint8_t i = 0; i < sizeof(sparks); i++)
			sparks[i] = 0;
		for (uint8_t i = 0; i < sizeof(params); i++)
			params[i] = 0;
	}

	PinInternal *allocPin() {
		assert(valid >= 0 && valid < MAX_PINS);
		return pins + (valid >= MAX_PINS ? 0 : valid++);
	}

	void initInjectorPin(uint8_t pin, uint8_t cyl) {
		maxinj = pin;
	}

	void initSparkPin(uint8_t pin, uint8_t cyl, uint8_t coils) {
		maxinj = pin;
		assert(cyl >= 0 && cyl < sizeof(sparks));

		if (cyl >= coils) {
			sparks[cyl] = sparks[0];
			return;
		}

		//coils == 1 ? cylinders : cyl + cylinders * 2;
		PinInternal *p = allocPin();
		sparks[cyl] = valid;
		p->initPin(pin, PinModeInput);
	}

public:

	Local() {
		clear();
	}

	inline void initEnginePins() {
		clear();

		uint8_t cyl = getParamUnsigned(ConstCylinders);
		uint8_t pin = 2;

		for (uint8_t i = 0; i < cyl; i++)
			initInjectorPin(pin++, i);

		uint8_t coils = getParamUnsigned(ConstCoils);

		for (uint8_t i = 0; i < cyl; i++)
			initSparkPin(pin++, i, coils);
	}

	inline void initPins(DevicePinDef *pins) {
		uint8_t n = 0;

		while (pins->mode) {
			PinInternal *p = allocPin();
			params[n++] = valid;
			p->initPin(pins++);
			assert(n < sizeof(params));
		}
	}

	inline PinInternal *getPinByIdx1(uint8_t id) {
		assert(id >= 1 && id <= valid);
		return pins + id - 1;
	}

	inline PinInternal *getPinById(uint8_t id) {
		for (uint8_t i = 0; i < valid; i++)
			if (pins[i].getId() == id)
				return pins + i;

		return 0;
	}

	inline PinInternal *getParamPin(uint8_t idx) {
		return getPinByIdx1(params[idx]);
	}

	inline uint8_t getSparkPin(uint8_t cyl) {
		int max = getParamUnsigned(ConstCoils);

		while (cyl && cyl >= max)
			cyl >>= 1;

		assert(cyl < sizeof(sparks));
		return sparks[cyl];
	}

	inline uint8_t getInjectorPin(uint8_t cyl) {
		int max = getParamUnsigned(ConstCylinders);

		while (cyl && cyl >= max)
			cyl >>= 1;

		assert(cyl < sizeof(injectors));
		return injectors[cyl];
	}

	inline void resetOutputPins() {
		for (uint8_t i = 0; i < sizeof(params) && params[i]; i++) {
			PinInternal *p = getParamPin(i);

			if (!p->isInput())
				p->clr();
		}
	}

	inline PinInternal *nextPin(uint8_t &idx, bool input) {
		for (uint8_t i = 0; i < sizeof(params) && params[i]; i++) {
			if (params[idx]) {
				PinInternal *p = getParamPin(idx++);

				if (input == p->isInput())
					return p;
			}

			if (!params[idx] || idx >= sizeof(params))
				idx = 0;
		}

		return 0;
	}

	inline PinInternal *getNextOutputPin() {
		return nextPin(output, false);
	}

	inline void addLast(PinInfo *next) {
		uint8_t type = next->getType();

		if (isInput()) {
			uint8_t n = 0;

			for (uint8_t i = 0; i < ARRSIZE(last); i++) {
				while (n < ARRSIZE(last) && last[n] && last[n]->getPin() == next->getPin())
					n++;

				last[i] = n >= ARRSIZE(last) ? 0 : last[n++];
			}

			for (uint8_t i = ARRSIZE(last) - 1; i > 0; i--)
				last[i] = last[i - 1];

			last[0] = next;
		}
	}
} local;

static void sendPins(void *data) {
	send.p1(F("pins"));
	send.json(F("valid"), local.valid);
	send.json(F("input"), local.input);
	send.json(F("output"), local.output);
	send.json(F("data"), (uint16_t) sizeof(Local));
	send.json(F("max"), Local::MAX_PINS);
	send.json(F("used"), local.valid);

	channel.p2();
	channel.nl();

	for (uint8_t i = 0; i < local.valid; i++) {
		PinInternal *p = local.pins + i;
		p->sendPin(i);
	}
}

void PinMgr::resetOutputPins() {
	local.resetOutputPins();
}

void PinMgr::readPins() {
	for (uint8_t i = 0; i < sizeof(local.params) && local.params[i]; i++) {
		PinInternal *p = local.getParamPin(i);
		p->readPin();
	}
}

PinInfo *PinMgr::getLast(uint8_t idx) {
	return idx >= ARRSIZE(local.last) ? 0 : local.last[idx];
}

uint16_t PinMgr::initPins(DevicePinDef *pins) {
	local.initPins(pins);

	static PromptCallback callbacks[] = {
		{ F("pins"), sendPins },
	};

	addPromptCallbacks(callbacks, ARRSIZE(callbacks));

	return sizeof(local) + sizeof(callbacks);
}

bool PinInfo::isInput() const {
	return (def->mode & PinModeInput) != 0;
}

uint32_t PinInfo::ms() const {
	return tdiff32(Epoch::millis(), def->changed);
}

void PinInfo::writePin(uint16_t v) {
	if (!isInput()) {
		if (def->last != v) {
			local.addLast(this);
			def->changed = Epoch::millis();
		}

		def->last = v;

		if (def->mode & PinModePWM)
			Device::analogWrite(def->id, v);
		else
			Device::digitalWrite(def->id, def->invert ? !v : v);
	}
}

void PinInfo::sendPin(uint8_t i) const {
	send.p1(F("pin"));
	send.json(F("i"), i);
	send.json(F("pin"), getPin());
	send.json(F("id"), getId());
	send.json(F("mode"), def->mode);
	send.json(F("val"), getVal());
	send.json(F("hyst"), def->hyst);
	send.json(F("mask"), getMask());
	send.json(F("port"), getPort() != 0);
	send.json(F("ms"), ms());
	send.json(getParamName(getId()), getPin());
	channel.p2();
	channel.nl();
}


PinInfo *PinMgr::getNextOutputPin() {
	return local.getNextOutputPin();
}

PinInfo *PinMgr::getPinById(uint8_t id) {
	return local.getPinById(id);
}

PinInfo *PinMgr::getPinByIdx1(uint8_t id) {
	return local.getPinByIdx1(id);
}

PinInfo *PinMgr::readNextInputPin() {
	PinInternal *next = local.nextPin(local.input, true);

	if (next && next->readPin()) {
		local.addLast(next);
		return next;
	}

	return 0;
}

#endif

GPIO::PinId PinMgr::getSparkPin(uint8_t cyl) {
	return (GPIO::PinId)(GPIO::Spark1 + cyl);
}

GPIO::PinId PinMgr::getInjectorPin(uint8_t cyl) {
	return (GPIO::PinId)(GPIO::Injector1 + cyl);
}
