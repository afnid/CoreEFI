uint16_t initTicks(void);
void sendTicks(void);

#ifdef ARDUINO_MEGA
extern "C" uint32_t micros(void);
#define clockTicks() micros()
#define delayTicks(us) delayMicroseconds(us)
#else
uint32_t clockTicks(void);
void delayTicks(uint32_t ticks);
#endif

#define tdiff32(a, b)		((int32_t)((uint32_t)(a) - (uint32_t)(b)))
#define tdiff16(a, b)		((int16_t)((uint16_t)(a) - (uint16_t)(b)))

#ifndef TICKTOUS
#define TICKTOUS 1
#endif

#if (TICKTOUS == 1)
#define MicrosToTicks(x) (x)
#define TicksToMicros(x) (x)
#else
#define MicrosToTicks(x) ((int32_t)(x) * TICKTOUS)
#define TicksToMicros(x) ((int32_t)(x) / TICKTOUS)
#endif
