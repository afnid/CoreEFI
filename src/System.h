// copyright to me, released under GPL V3

#ifndef _System_h_
#define _System_h_

#include "Buffer.h"

void toggleled(uint8_t id);

// Portability

#ifdef ARDUINO_MEGA
typedef uint16_t pulse_t;
#else
typedef uint32_t pulse_t;
#endif

void initSystem(bool doefi);

#endif
