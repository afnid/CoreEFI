#include "System.h"

#ifdef STM32

#if 0

volatile uint32_t *DWT_CYCCNT = (volatile uint32_t *) 0xE0001004; //address of the register
volatile uint32_t *DWT_CONTROL = (volatile uint32_t *) 0xE0001000; //address of the register
volatile uint32_t *SCB_DEMCR = (volatile uint32_t *) 0xE000EDFC; //address of the register

uint16_t initTicks(void) {
	*SCB_DEMCR = *SCB_DEMCR | 0x01000000;
	*DWT_CYCCNT = 0; // reset the counter
	*DWT_CONTROL = *DWT_CONTROL | 1; // enable the counter
	return 0;
}

uint32_t clock_ticks() {
	return *DWT_CYCCNT / 168;
}

#else

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_rcc.h"

uint16_t initTicks(void) {
	extern void initTimer(TIM_TypeDef *tim);
	initTimer(TIM2);
	TIM2->ARR = -1;
	TIM2->CNT = HAL_GetTick() * 1000;
	return 0;
}

uint32_t clock_ticks(void) {
	return TIM2->CNT;
}

#endif

void delay_ticks(uint32_t ticks) {
	uint32_t stop = clock_ticks() + ticks;
	uint32_t now = 0;

	do {
		now = clock_ticks();
	} while (tdiff32(now, stop) < 0);
}

#elif !defined(ARDUINO)

#include <time.h>

#define NanosToTicks(x) ((x) / (1000 / TICKTOUS))
#define TicksToNanos(x) ((x) * (1000 / TICKTOUS))

#ifdef CLOCK_MONOTONIC
uint32_t clock_ticks(void) {
	static uint32_t offset = (1L << 32) - 5;
	static uint32_t sec = 0;
	static struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	if (sec == 0)
		sec = ts.tv_sec;

	ts.tv_sec -= sec;
	ts.tv_sec += offset;

	return NanosToTicks(ts.tv_sec * 1000000000ULL + ts.tv_nsec);
}

#else
uint32_t clock_ticks(void) {
	static uint32_t offset = (1L << 32) - 5;
	static uint32_t sec = 0;
	static struct timeval tv;
	gettimeofday(&tv, 0);

	if (sec == 0)
		sec = tv.tv_sec;

	tv.tv_sec -= sec;
	tv.tv_sec += offset;

	return MicrosToTicks(tv.tv_sec * 1000000ULL + tv.tv_usec);
}

#endif

uint16_t initTicks(void) {
	return 0;
}

void delay_ticks(uint32_t ticks) {
	static struct timespec req;
	static struct timespec rem;

	myzero(&req, sizeof(req));
	myzero(&rem, sizeof(rem));
	req.tv_nsec = TicksToNanos(ticks);
	req.tv_sec = req.tv_nsec / 1000000000ULL;
	req.tv_nsec -= req.tv_sec * 1000000000ULL;

	do {
		nanosleep(&req, &rem);
	} while (rem.tv_sec > 0 || rem.tv_nsec > 0);
}

#endif
