#ifndef _Epoch_h_
#define _Epoch_h_

#include "utils.h"

class Epoch {
	static uint32_t counter;
	static uint32_t last;

public:

	static void tick(uint32_t now);

	static void init();

	static uint16_t mem(bool alloced);

	static uint32_t seconds();

	static uint32_t millis();

	static uint32_t micros();

	static uint32_t ticks();
};

#endif
