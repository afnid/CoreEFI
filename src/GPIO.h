#ifndef _GPIO_h_
#define _GPIO_h_

#include "utils.h"

#include "Pins.h"

class GPIO {
public:

#include "efi_gpio.h"

	typedef struct {
		uint8_t ext;
		uint8_t id;
		const flash_t *name;
		uint16_t mode;
		uint8_t def;
		uint8_t invert;
		uint16_t hyst;
		uint16_t last;
		uint32_t changed;
		char info[6];

		PinId getId() const {
			return (PinId)id;
		}

		inline uint16_t getLast() const {
			return last;
		}

		inline uint32_t ms() const {
			return tdiff32(millis(), changed);
		}
	} PinDef;

	static const PinDef *getPinDef(PinId id);

	static void setPin(PinId id, bool v);
	static bool isPinSet(PinId id);

	static uint16_t analogRead(PinId id);
	static void analogWrite(PinId id, uint16_t v);

	static void init();
};

#endif
