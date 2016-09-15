#include "System.h"
#include "Buffer.h"

#ifndef UNIX

void delayTicks(uint32_t ticks) {
	uint32_t stop = clockTicks() + ticks;
	uint32_t now = 0;

	do {
		now = clockTicks();
	} while (tdiff32(now, stop) < 0);
}

#endif

#ifdef STM32

#include "st_main.h"

static const uint8_t timebits = 16;
static const uint32_t timemask = (1L << timebits) - 1;
static uint16_t dummy;
static uint16_t &rollovers = dummy;

uint16_t initTicks(void) {
	//extern uint16_t *initTickTimer(TIM_TypeDef *tim, uint32_t per);
	//rollovers = *initTickTimer(TIM2, 1L << timebits);
	//TIM2->ARR = -1;
	//TIM2->CNT = HAL_GetTick() * 1000;
	cycle.init();
	return 0;
}

//uint32_t clockTicks(void) {
	//return TIM2->CNT; //(rollovers << timebits) | (TIM2->CNT & timemask);
//}

uint32_t clockTicks2() {
	return HAL_GetTick() * 1000;
}

#elif defined(ARDUINO_DUE)

// maybe 2x as fast?

#include <Arduino.h>

static const uint32_t dueratio = VARIANT_MCK / 1000 / 1000 / 2;
static const uint8_t timebits = 24;
static const uint32_t rollmask = (1UL << timebits) - 1;
static uint32_t rollovers = 0;

//void TC3_Handler() {
	//TC_GetStatus(TC1, 0);
	//rollovers += (1UL << timebits);
//}

uint16_t initTicks(void) {
	//extern void initTickTimer(uint8_t, uint32_t us);
	//initTickTimer(3, 1 << timebits);
	return 0;
}

//uint32_t clockTicks(void) {
	//uint32_t ticks = TC_ReadCV(TC1, 0) / dueratio;
	//assert((ticks & rollmask) == ticks);
	//return rollovers | ticks;
//}

uint32_t clockTicks2(void) {
	return micros();
}

#elif defined(ARDUINO)

#include <Arduino.h>

uint32_t CycleCount::ticks(void) {
	return micros();
}

uint32_t clockTicks2(void) {
	return micros();
}

uint16_t initTicks(void) {
	return 0;
}

#else

#include <time.h>

#define NanosToTicks(x) ((x) / (1000 / TICKTOUS))
#define TicksToNanos(x) ((x) * (1000 / TICKTOUS))

static const uint64_t nstos = (uint64_t)1000000000;

#ifdef CLOCK_MONOTONIC
uint32_t clockTicks() {
	static uint32_t epoch = (1L << 32) - 5;
	static uint32_t sec = 0;
	static struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	if (sec == 0)
		sec = ts.tv_sec;

	ts.tv_sec -= sec;
	ts.tv_sec += epoch;

	return NanosToTicks(ts.tv_sec * nstos + ts.tv_nsec);
}

#elif defined(ARDUINO)
uint32_t CycleCount::ticks(void) {
	return micros();
}

#else
uint32_t CycleCount::ticks(void) {
	static uint32_t epoch = (1L << 32) - 5;
	static uint32_t sec = 0;
	static struct timeval tv;
	gettimeofday(&tv, 0);

	if (sec == 0)
		sec = tv.tv_sec;

	tv.tv_sec -= sec;
	tv.tv_sec += epoch;

	return MicrosToTicks(tv.tv_sec * nstos + tv.tv_usec);
}

#endif

void resetCycleCount(void) {
}

uint32_t clockTicks2(void) {
	return clockTicks();
}

uint32_t getCycleCount(void) {
	return 0;
}

uint16_t initTicks(void) {
	return 0;
}

void delayTicks(uint32_t ticks) {
	static struct timespec req;
	static struct timespec rem;

	myzero(&req, sizeof(req));
	myzero(&rem, sizeof(rem));
	req.tv_nsec = TicksToNanos(ticks);
	req.tv_sec = req.tv_nsec / nstos;
	req.tv_nsec -= req.tv_sec * nstos;

	do {
		nanosleep(&req, &rem);
	} while (rem.tv_sec > 0 || rem.tv_nsec > 0);
}

#endif

void sendTicks(Buffer &send) {
#if defined(ARDUINO_DUE) || defined(STM32)
	static uint32_t last1 = 0;
	static uint32_t last2 = 0;

	uint32_t t1 = clockTicks2();
	uint32_t t2 = TicksToMicros(cycle.ticks());

	if (t1 && t2) {
		int32_t d1 = tdiff32(t1, last1);
		int32_t d2 = tdiff32(t2, last2);
		send.p1(F("ticks"));
		send.json(F("t1"), t1);
		send.json(F("t2"), t2);
		send.json(F("drift"), d2 - d1, false);
		send.p2();
	}

	last1 = t1;
	last2 = t2;
#endif
}
