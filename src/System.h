// copyright to me, released under GPL V3

#include <stdint.h>
#include <stdlib.h>

#ifdef ARDUINO_AVR_MEGA2560
#define ARDUINO_MEGA
#endif

#ifdef ARDUINO_ARCH_SAM
#define ARDUINO_DUE
#endif

#if !defined(ARDUINO) && !defined(STM32)
#define UNIX 1
#endif

#include "Ticks.h"

// Utilities

#define min(a, b)	((a) < (b) ? a : b)
#define max(a, b)	((a) > (b) ? a : b)

#define bitsize(n) (((n) + 7) >> 3)
#define bitmask(n) (1 << ((n) & 7))
#define bitidx(n) ((n) >> 3)
#define isset(a, n) ((a[bitidx(n)] & bitmask(n)) != 0)
#define bitset(a, n) (a[bitidx(n)] |= bitmask(n))
#define bitclr(a, n) (a[bitidx(n)] &= ~bitmask(n))

void myzero(void *p, uint16_t len);
void toggleled(uint8_t id);

// Portability

#ifdef STM32
#define NDEBUG
#endif

#ifdef ARDUINO
#define NDEBUG
#define DEBUG

typedef uint16_t pulse_t;

#else

typedef uint32_t pulse_t;

#ifndef abs
#define abs(x)		((x) < 0 ? -(x) : (x))
#endif

#ifndef bit
#define bit(x)	(1 << (x))
#endif

#define F(x)	(x)

#define F_CPU (168 * 1000L * 1000L)

#endif

#include <assert.h>

void initSystem();
