#ifndef _Ticks_h_
#define _Ticks_h_

uint16_t initTicks(void);
void sendTicks(Buffer &send);

class CycleCount {
#if defined(STM32) || defined(ARDUINO_DUE)
	volatile uint32_t *DWT_CYCCNT;
	volatile uint32_t *DWT_CONTROL;
	volatile uint32_t *SCB_DEMCR; // debugg exception and monitor control

public:

	CycleCount() {
		DWT_CYCCNT = (volatile uint32_t *) 0xE0001004;
		DWT_CONTROL = (volatile uint32_t *) 0xE0001000;
		SCB_DEMCR = (volatile uint32_t *) 0xE000EDFC;
	}

	inline uint16_t init(void) {
		*SCB_DEMCR |= 0x01000000;
		*DWT_CYCCNT = 0; // reset the counter
		*DWT_CONTROL |= 1; // enable the counter
		return sizeof(CycleCount);
	}

	inline uint32_t ticks() {
		return *DWT_CYCCNT;
	}

	inline uint32_t micros() {
		static const uint16_t DIV = F_CPU / 1000 / 1000;
		return *DWT_CYCCNT / DIV; // TODO: will skip when wrapping, only use to check drift!
	}

	inline void reset() {
		*DWT_CYCCNT = 0;
	}
#else

public:

	CycleCount() {
	}

	inline uint16_t init(void) {
		return 0;
	}

	inline void reset(void) {
	}

	uint32_t ticks();

	inline uint32_t micros() {
		return TicksToMicros(ticks());
	}

#endif
};

extern CycleCount cycle;

#endif
