#include <stdint.h>
#include <stdlib.h>

#ifdef ARDUINO_AVR_MEGA2560
#define ARDUINO_MEGA
#endif

#ifdef ARDUINO_ARCH_SAM
#define ARDUINO_DUE
#endif

// Utilities

#define tdiff32(a, b)		((int32_t)((uint32_t)(a) - (uint32_t)(b)))
#define tdiff16(a, b)		((int16_t)((uint16_t)(a) - (uint16_t)(b)))

#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))

#define bitsize(n) (((n) + 7) >> 3)
#define bitmask(n) (1 << ((n) & 7))
#define bitidx(n) ((n) >> 3)
#define isset(a, n) ((a[bitidx(n)] & bitmask(n)) != 0)
#define bitset(a, n) (a[bitidx(n)] |= bitmask(n))
#define bitclr(a, n) (a[bitidx(n)] &= ~bitmask(n))

void myzero(void *p, uint16_t len);

// Portability

#ifdef ARDUINO
#define NDEBUG
#define DEBUG

typedef uint16_t pulse_t;
#define clock_ticks() micros()
#define delay_ticks(x) delayMicroseconds(x)

#define MicrosToTicks(x) (x)
#define TicksToMicros(x) (x)

extern "C" uint32_t micros();

#else

#define TICKTOUS 1
#define MicrosToTicks(x) ((x) * TICKTOUS)
#define TicksToMicros(x) ((x) / TICKTOUS)

typedef uint32_t pulse_t;

#ifndef abs
#define abs(x)		((x) < 0 ? -(x) : (x))
#endif

#ifndef bit
#define bit(x)	(1 << (x))
#endif

#define F(x)	(x)

#define F_CPU (168 * 1000L * 1000L)

uint32_t clock_ticks();
void delay_ticks(uint32_t ticks);

uint32_t micros();
void _delay_us(uint16_t us);

#endif

#include <assert.h>

void initSystem();
