#define tdiff32(a, b)		((int32_t)((uint32_t)(a) - (uint32_t)(b)))
#define tdiff16(a, b)		((int16_t)((uint16_t)(a) - (uint16_t)(b)))

#if defined(ARDUINO)

#define TICKTOUS 1

#define clock_ticks() micros()
#define delay_ticks(x) delayMicroseconds(x)
#define init_ticks()

extern "C" uint32_t micros();

#else

#define TICKTOUS 1

uint16_t initTicks();
uint32_t clock_ticks();
void delay_ticks(uint32_t ticks);

#endif

#if (TICKTOUS == 1)
#define MicrosToTicks(x) (x)
#define TicksToMicros(x) (x)
#else
#define MicrosToTicks(x) ((int32_t)(x) * TICKTOUS)
#define TicksToMicros(x) ((int32_t)(x) / TICKTOUS)
#endif
