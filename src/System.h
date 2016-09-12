// copyright to me, released under GPL V3

#ifndef _System_h_
#define _System_h_

#include "utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO_AVR_UNO
#define ARDUINO_UNO
#endif

#ifdef ARDUINO_AVR_MEGA2560
#define ARDUINO_MEGA
#endif

#ifdef ARDUINO_ARCH_SAM
#define ARDUINO_DUE
#endif

#ifdef ARDUINO
#define NDEBUG
#endif

#if !defined(ARDUINO) && !defined(STM32)
#define UNIX 1
#endif

// Utilities

// bitmaps

#define bitsize(n) (((n) + 7) >> 3)
#define bitmask(n) (1 << ((n) & 7))
#define bitidx(n) ((n) >> 3)
#define isset(a, n) ((a[bitidx(n)] & bitmask(n)) != 0)
#define bitset(a, n) (a[bitidx(n)] |= bitmask(n))
#define bitclr(a, n) (a[bitidx(n)] &= ~bitmask(n))

void myzero(void *p, uint16_t len);
void toggleled(uint8_t id);

// Portability

#ifdef ARDUINO_MEGA
typedef uint16_t pulse_t;
#else
typedef uint32_t pulse_t;
#endif

#ifndef ARDUINO

#ifndef abs
#define abs(x)		((x) < 0 ? -(x) : (x))
#endif

#ifndef bit
#define bit(x)	(1 << (x))
#endif

#define F(x)	(x)

#endif

#ifndef F_CPU
#ifdef STM32
#define F_CPU 168000000L
#else
#define F_CPU 1000000L
#endif
#endif

#include "Ticks.h"
#include <assert.h>

void initSystem(bool doefi);

#endif
