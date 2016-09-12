#ifndef _GPIO_h_
#define _GPIO_h_

#include "utils.h"

class GPIO {
public:

#include "efi_gpio.h"

	typedef enum {
		PinModePullup = (1 << 0),
		PinModeAnalog = (1 << 1),
		PinModeInput = (1 << 2),
		PinModeOutput = (1 << 3),
		PinModePWM = (1 << 4),
		PinModeNoInit = (1 << 5),
		PinModeChanges = (1 << 6),
		PinModeEdge = (1 << 7)
	} PinMode;

	typedef struct {
		uint8_t pin;
		uint8_t id;
		const char *name;
		uint16_t mode;
		uint16_t hyst;
		uint16_t last;
		uint32_t changed;
		uint8_t def;
		uint8_t invert;
		char info[6];

		inline PinId getId() const {
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
