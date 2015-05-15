class PinInfo {
	volatile uint8_t *port;
	uint8_t mask;
	uint8_t pin;

public:

	void digitalWrite(uint8_t v) const volatile;
	uint16_t digitalRead() const volatile;
	uint16_t analogRead() const volatile;

	inline void init(uint8_t pin, uint8_t mask, volatile uint8_t *port) volatile {
		this->pin = pin;
		this->mask = mask;
		this->port = port;
	}

	inline uint8_t getPin() const volatile {
		return pin;
	}

	inline uint8_t getMask() const volatile {
		return mask;
	}

	inline volatile uint8_t *getPort() const volatile {
		return port;
	}

	inline void set() volatile {
		if (port && mask) {
			assert(mask);
			*port |= mask;
		} else
			digitalWrite(true);
	}

	inline void clr() volatile {
		if (port && mask) {
			assert(mask);
			*port &= ~mask;
		} else
			digitalWrite(false);
	}

	inline uint16_t isSet() const volatile {
		if (port && mask) {
			assert(mask);
			return (*port & mask) != 0;
		}

		return digitalRead();
	}
};

uint16_t initPins();
void sendPins();
void readPins();

volatile PinInfo *getPin(uint8_t id);
volatile PinInfo *getParamPin(uint8_t id);
uint8_t getInjectorPin(uint8_t cyl);
uint8_t getSparkPin(uint8_t cyl);
void resetOutputPins();
