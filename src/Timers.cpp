// copyright to me, released under GPL V3

#include "System.h"
#include "Buffer.h"
#include "Broker.h"
#include "Metrics.h"
#include "Params.h"

#include "Decoder.h"
#include "Encoder.h"
#include "Timers.h"
#include "Events.h"

static const char *PATH = __FILE__;

#if defined(STM32) || defined(ARDUINO_DUE)
//#define TIMER_TEST
#endif

// Timers 0-3 valid on uno, 0, 2 are 8 bit
// Timers 0-5 valid on mega, 0, 2 are 8 bit
// Timers 0-8 valid on due, all 32 bit

enum {
	TimerId2,
	TimerId5,
	// first too are always used unless TIMER_TEST is defined
	MaxTimers, // after here are disabled..
	TimerId4,

	TimerId1,
	TimerId3,

	TimerId6,
	TimerId7,
	TimerId0,
	TimerId8, // TODO: do not use, locked up?
};

static void handleTimer(uint8_t id);

#ifdef STM32F4

static const uint16_t DEFSLOP = MicrosToTicks(1);
static const uint16_t MAXSLOP = MicrosToTicks(5);
static const uint16_t MAXJITTER = MicrosToTicks(10);
static const uint8_t MAXLOOPS = 4;

#include "st_main.h"

static int stprio = 0;

class FastTimer {
	void enable() volatile {
		//tim->CNT = 0;

		if (apb == 1)
			RCC->APB1ENR |= mask;
		else
			RCC->APB2ENR |= mask;
	}

	void disable() volatile {
		if (apb == 1)
			RCC->APB1ENR &= ~mask;
		else
			RCC->APB2ENR &= ~mask;
	}

	uint32_t getCount() volatile {
		return tim->CNT;
	}

	uint32_t getFreq() volatile {
		switch (clock) {
			case 0:
				return HAL_RCC_GetHCLKFreq();
			case 1:
				return HAL_RCC_GetPCLK1Freq();
			case 2:
				return HAL_RCC_GetPCLK2Freq();
			default:
				return 0;
		}
	}

	int isEnabled() volatile {
		if (apb == 1)
			return (RCC->APB1ENR & mask) != 0;
		return (RCC->APB2ENR & mask) != 0;
	}

	enum {
		CountInit,
		CountStart,
		CountStop,
		CountNotify,
		CountMsp,
		HistMax,
	};

	inline void addHist(uint8_t h) volatile {
		if (h < HistMax && hist[h] + 1 != 0)
			hist[h]++;
	}

public:
	TIM_HandleTypeDef handle;
	TIM_TypeDef *tim;
	volatile uint8_t bits;
	volatile uint8_t clock;
	volatile uint8_t apb;
	volatile uint8_t irq;
	volatile uint32_t mask;
	volatile uint16_t div;
	volatile uint16_t ints;
	volatile uint8_t state;
	volatile uint16_t hist[HistMax];
	volatile uint32_t ticks;

	void init() {
		if (!state) {
			div = getFreq() / 1000L / 1000L;
			div = 4;

			myzero(&handle, sizeof(handle));
			handle.Instance = tim;

			handle.Init.Period = MicrosToTicks(1000000L) / div - 1;
			handle.Init.Prescaler = 1;
			handle.Init.CounterMode = TIM_COUNTERMODE_UP;

			if (HAL_TIM_Base_Init(&handle) != HAL_OK)
				myerror();

			if (HAL_TIM_Base_Start_IT(&handle) != HAL_OK)
				myerror();

			addHist(CountInit);

			state = 1;
		}
	}

	void start(int32_t ticks) {
		static const int32_t minticks = MicrosToTicks(1);

		this->ticks = ticks;
		ticks = ticks / 4 - 1;
		ticks = max(ticks, minticks);

		__HAL_TIM_SET_AUTORELOAD(&handle, ticks);

		if (HAL_TIM_Base_Start_IT(&handle) != HAL_OK)
			myerror();

		addHist(CountStart);
		state = 2;
	}

	void stop() {
		// enbable/disable work, but can't write arr register without this..

		if (HAL_TIM_Base_Stop_IT(&handle) != HAL_OK)
			myerror();

		addHist(CountStop);
		state = 3;
	}

	void notifyHal() {
		if (handle.Instance)
			HAL_TIM_IRQHandler(&handle);
		else
			myerror();

		addHist(CountNotify);
		ints++;
	}

	void mspinit() volatile {
		enable();

		if (irq) {
			HAL_NVIC_SetPriority((IRQn_Type) irq, stprio, stprio);
			HAL_NVIC_EnableIRQ((IRQn_Type) irq);
			stprio++;
		}

		addHist(CountMsp);
	}

	void send(Buffer &send, uint8_t i) {
		send.p1(F("tim"));
		send.json("i", i);
		send.json("state", state);
		send.json("id", ++i);
		send.json("bits", bits);
		send.json("clk", clock);
		send.json("apb", apb);
		send.json("en", (uint8_t) isEnabled());
		send.json("rq", irq);
		send.json("per", handle.Init.Period);
		send.json("arr", tim->ARR);
		send.json("psc", tim->PSC);
		send.json("cnt", tim->CNT);
		send.json("ticks", ticks);
		sendHist(send, hist, HistMax);
		send.p2();
	}
};

static FastTimer stm32timers[] = {
	{ { }, TIM1, 16, 0, 2, 0, RCC_APB2ENR_TIM1EN, 0, 0, 0 },
	{ { }, TIM2, 32, 2, 1, TIM2_IRQn, RCC_APB1ENR_TIM2EN, 0, 0, 0 },
	{ { }, TIM3, 16, 2, 1, TIM3_IRQn, RCC_APB1ENR_TIM3EN, 0, 0, 0 },
	{ { }, TIM4, 16, 2, 1, TIM4_IRQn, RCC_APB1ENR_TIM4EN, 0, 0, 0 },
	{ { }, TIM5, 32, 2, 1, TIM5_IRQn, RCC_APB1ENR_TIM5EN, 0, 0, 0 },
	{ { }, TIM6, 16, 2, 1, 0, RCC_APB1ENR_TIM6EN, 0, 0, 0 },
	{ { }, TIM7, 16, 2, 1, TIM7_IRQn, RCC_APB1ENR_TIM7EN, 0, 0, 0 },
	{ { }, TIM8, 16, 0, 2, 0, RCC_APB2ENR_TIM8EN, 0, 0, 0 },
	{ { }, TIM9, 16, 0, 2, 0, RCC_APB2ENR_TIM9EN, 0, 0, 0 },
	{ { }, TIM10, 16, 0, 2, 0, RCC_APB2ENR_TIM10EN, 0, 0, 0 },
	{ { }, TIM11, 16, 0, 2, 0, RCC_APB2ENR_TIM11EN, 0, 0, 0 },
	{ { }, TIM12, 16, 2, 1, 0, RCC_APB1ENR_TIM12EN, 0, 0, 0 },
	{ { }, TIM13, 16, 2, 1, 0, RCC_APB1ENR_TIM13EN, 0, 0, 0 },
	{ { }, TIM14, 16, 2, 1, 0, RCC_APB1ENR_TIM14EN, 0, 0, 0 }
	// all timers added for completeness, turned them all on!
	// 2 is used for my clock timer, and 3 is used by usb system
};

static const int ntimers = sizeof(stm32timers) / sizeof(stm32timers[0]);

static FastTimer *getFastTimer(uint8_t id) {
	switch (id) {
		case TimerId2:
			return stm32timers + 1;
		case TimerId3:
			return stm32timers + 2;
		case TimerId4:
			return stm32timers + 3;
		case TimerId5:
			return stm32timers + 4;
		case TimerId6:
			return stm32timers + 5;
		case TimerId7:
			return stm32timers + 6;
		default:
			return 0;
	}
}

#define myHandler(idx, id)	extern "C" void TIM##id##_IRQHandler() {\
		stm32timers[idx].notifyHal();\
		handleTimer(TimerId##id);\
}

#define myHandler2(idx, id)	extern "C" void TIM##id##_IRQHandler() {\
		stm32timers[idx].notifyHal();\
}

myHandler(0, 1)
myHandler(1, 2)
//myHandler(2, 3)
myHandler(3, 4)
myHandler(4, 5)
myHandler(5, 6)
myHandler(6, 7)
myHandler(7, 8)
myHandler2(8, 9)
myHandler2(9, 10)
myHandler2(10, 11)
myHandler2(11, 12)
myHandler2(12, 13)
myHandler2(13, 14)
myHandler2(14, 15)

static int8_t getTimerId(TIM_TypeDef *tim) {
	if (tim)
		for (int8_t i = 0; i < ntimers; i++)
			if (stm32timers[i].tim == tim)
				return i;
	return -1;
}

volatile uint16_t *initTickTimer(TIM_TypeDef *tim, uint32_t us) {
	int8_t id = getTimerId(tim);

	if (id >= 0) {
		FastTimer *t = stm32timers + id;
		t->init();
		t->stop();
		t->start(us);
		return &t->ints;
	}

	myerror();

	return &stm32timers[0].ints;
}

extern "C" void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *h) {
	int id = getTimerId(h->Instance);

	if (id >= 0)
		stm32timers[id].mspinit();
	else
		myerror();
}

void initTimers2() {
	for (int i = 0; i < ntimers; i++)
		if (i != 2 && i != 5)
			stm32timers[i].init();
}

void startTimers() {
	for (int i = 0; i < ntimers; i++)
		if (i != 2 && i != 5)
			stm32timers[i].start(25000L);
}

void stopTimers() {
	for (int i = 0; i < ntimers; i++)
		if (i != 2 && i != 5)
			stm32timers[i].stop();
}

static void printTimers(Buffer &send) {
	float div = 1000 * 1000;

	send.p1("clocks");
	send.json("hclk", HAL_RCC_GetHCLKFreq() / div);
	send.json("pclk1", HAL_RCC_GetPCLK1Freq() / div);
	send.json("pclk2", HAL_RCC_GetPCLK2Freq() / div);
	send.p2();

	for (int i = 0; i < ntimers; i++)
		stm32timers[i].send(send, i);
}

#elif defined(ARDUINO_DUE)
#include <Arduino.h>
static const uint16_t DEFSLOP = MicrosToTicks(1);
static const uint16_t MAXSLOP = MicrosToTicks(10);
static const uint16_t MAXJITTER = MicrosToTicks(15);
static const uint8_t MAXLOOPS = 4;

#if 1
static const uint32_t dueratio = VARIANT_MCK / 1000 / 1000 / 2;

typedef struct {
	Tc *tc;
	uint32_t chn;
	IRQn_Type irq;
	uint32_t ticks;
	TcChannel *ch;

	void init() volatile {
		ch = tc->TC_CHANNEL + chn;
		pmc_set_writeprotect(false);
		pmc_enable_periph_clk((uint32_t)irq);
		TC_Configure(tc, chn, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK1);
		ch->TC_IER = TC_IER_CPCS;
		ch->TC_IDR = ~TC_IER_CPCS;
		NVIC_ClearPendingIRQ(irq); // NVIC functions are already inline.
		NVIC_EnableIRQ(irq);
	}

	inline void start(uint16_t prescale, uint32_t ticks) {
		this->ticks = ticks;
		ticks >>= 1;
		ticks = max(ticks, 2);
		ch->TC_RC = ticks - 1; // TC_SetRC(tc, chn, ticks - 1); //TC_SetRC(tc, chn, dueratio * ticks - 1);
		ch->TC_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG; // TC_Start(tc, chn);
	}

	inline void stop() {
		//NVIC_DisableIRQ(irq);
		ch->TC_CCR = TC_CCR_CLKDIS; // TC_Stop(tc, chn);
	}

	inline uint32_t status() {
		return ch->TC_SR;
	}

	inline void json(Buffer &send, uint8_t i) {
		send.p1(F("tim"));
		send.json(F("i"), i);
		send.json(F("chn"), chn);
		send.json(F("irq"), (uint32_t)irq);
		send.json(F("rc"), ticks);
		//send.json(F("status"), (uint32_t)TC_GetStatus(tc, chan));
		send.json(F("cv"), (uint32_t)TC_ReadCV(tc, chn));
		send.p2();
	}
} FastTimer;

uint16_t getPrescaleBits(uint8_t idx, uint32_t ticks)  {
	return 0;
}

static FastTimer duetimers[] = {
	{ TC0, 0, TC0_IRQn },
	{ TC0, 1, TC1_IRQn },
	{ TC0, 2, TC2_IRQn },
	{ TC1, 0, TC3_IRQn },
	{ TC1, 1, TC4_IRQn },
	{ TC1, 2, TC5_IRQn },
	{ TC2, 0, TC6_IRQn },
	{ TC2, 1, TC7_IRQn },
	{ TC2, 2, TC8_IRQn },
};

static const int ntimers = sizeof(duetimers) / sizeof(duetimers[0]);

#define myHandler2(id, tc, c)	void TC##id##_Handler() { duetimers[id].status(); handleTimer(TimerId##id); }

myHandler2(0, 0, 0)
myHandler2(1, 0, 1)
myHandler2(2, 0, 2)
//myHandler2(3, 1, 0)
myHandler2(4, 1, 1)
myHandler2(5, 1, 2)
myHandler2(6, 2, 0)
myHandler2(7, 2, 1)
myHandler2(8, 2, 2)

static FastTimer *getFastTimer(uint8_t id) {
	switch (id) {
		case TimerId0:
			return duetimers + 0;
		case TimerId1:
			return duetimers + 1;
		case TimerId2:
			return duetimers + 2;
		case TimerId3:
			return duetimers + 3;
		case TimerId4:
			return duetimers + 4;
		case TimerId5:
			return duetimers + 5;
		case TimerId6:
			return duetimers + 6;
		case TimerId7:
			return duetimers + 7;
		case TimerId8:
			return duetimers + 8;
		default:
			return 0;
	}
}

void initTickTimer(uint8_t id, uint32_t us)
{
	duetimers[id].init();
	duetimers[id].start(0, us);
}

static void printTimers(Buffer &send) {
	float div = 1000 * 1000;

	send.p1(F("clocks"));
	send.json(F("ratio"), dueratio);
	send.p2();

	for (int i = 0; i < ntimers; i++)
		duetimers[i].json(send, i);
}

#else

// too much floating point, slop at 122 us with 4 timers, mine is 5 us, DueTimer could be written way more efficiently, but I need every us!
// left in to go back and optimize duetimer .. maybe
#include "DueTimer.h"

#define dueHandler(id)	static void handleTimer##id() { handleTimer(TimerId##id); }

dueHandler(1)
dueHandler(2)
dueHandler(3)
dueHandler(4)
dueHandler(5)
dueHandler(6)
dueHandler(7)
dueHandler(8)

#define FastTimer DueTimer

static FastTimer *getFastTimer(uint8_t id) {
	FastTimer *t = 0;

	switch (id) {
		case TimerId1:
			t = &Timer1;
			t->attachInterrupt(handleTimer1);
			break;
		case TimerId2:
			t = &Timer2;
			t->attachInterrupt(handleTimer2);
			break;
		case TimerId3:
			t = &Timer3;
			t->attachInterrupt(handleTimer3);
			break;
		case TimerId4:
			t = &Timer4;
			t->attachInterrupt(handleTimer4);
			break;
		case TimerId5:
			t = &Timer5;
			t->attachInterrupt(handleTimer5);
			break;
		case TimerId6:
			t = &Timer6;
			t->attachInterrupt(handleTimer6);
			break;
		case TimerId7:
			t = &Timer7;
			t->attachInterrupt(handleTimer7);
			break;
		case TimerId8:
			t = &Timer8;
			t->attachInterrupt(handleTimer8);
			break;
	}

	return t;
}

#endif

#elif ARDUINO // MEGA
#include <Arduino.h>
// TODO: had < 2% error up to 2k rpm, but broke it when i optimized for due and added the additional calls for current time
static const uint16_t DEFSLOP = MicrosToTicks(90);
static const uint16_t MAXSLOP = MicrosToTicks(255);
static const uint16_t MAXJITTER = MicrosToTicks(50);
static const uint8_t MAXLOOPS = 1;

static inline uint8_t getPrescaleBits(uint8_t id, int32_t &us) {
	static const uint16_t TIME_RATIO = F_CPU / 1000 / 1000; // 16
	static const uint16_t max16 = 65535u;
	static const uint16_t max8 = 255;

	uint16_t max = id == TimerId2 || id == TimerId0 ? max8 : max16;
	us = max(us, 1);// TODO: bump this up for lockups
	us = us * TIME_RATIO - 1;

	if (us <= max)// heavily modified/fixed from timer1/timer3
		return 0;
	if ((us >>= 3) <= max)
		return 3;
	if ((us >>= 3) <= max)
		return 6;
	if ((us >>= 2) <= max)
		return 8;
	if ((us >>= 2) <= max)
		return 10;

	us = max - 1;
	return 10;
}

static inline uint8_t mask(uint8_t id, uint8_t shift, uint8_t b0, uint8_t b1, uint8_t b2) {
	if (id == TimerId2)
	switch (shift) { // pg 191
		case 0:
			return bit(b0);
		case 3:
			return bit(b1);
		case 5:
			return bit(b1) | bit(b0);
		case 6:
			return bit(b2);
		case 7:
			return bit(b2) | bit(b0);
		case 8:
			return bit(b2) | bit(b1);
		case 10:
		default:
			return bit(b2) | bit(b1) | bit(b0);
	}

	switch (shift) {
		case 0: // pg 134 for 0, 161 for the rest
			return bit(b0);
		case 3:
			return bit(b1);
		case 6:
			return bit(b1) | bit(b0);
		case 8:
			return bit(b2);
		case 10:
		default:
			return bit(b2) | bit(b0);
	}
}

typedef struct {
	void (*initTimer)();
	void (*startTimer)(uint8_t bits, uint32_t us);

	inline void init() {
		initTimer();
	}

	inline void start(uint8_t bits, uint32_t us) {
		startTimer(bits, us);
	}

	inline void stop() {
	}
} FastTimer;

// TODO: timers below 40 us sometimes queue up before the timer can be stopped .. no known solution
#define oneShotTimer(id)\
static void initTimer##id() {\
	TCCR##id##A = 0;\
	TIMSK##id = bit(OCIE##id##A);\
}\
static void startTimer##id(uint8_t bits, uint32_t us) {\
	TCNT##id = 0;\
	OCR##id##A = us;\
	TCCR##id##B = bit(WGM##id##2) | mask(TimerId##id, bits, CS##id##0, CS##id##1, CS##id##2);\
}\
ISR(TIMER##id##_COMPA_vect) { TCCR##id##B = 0; handleTimer(TimerId##id); }

oneShotTimer(0);
oneShotTimer(1);
oneShotTimer(2);

#ifdef ARDUINO_MEGA
oneShotTimer(3);
oneShotTimer(4);
oneShotTimer(5);
#endif

static FastTimer *getFastTimer(uint8_t id) {
	static FastTimer timers[] = {
		{ initTimer0, startTimer0 }, // 8 bit
		{ initTimer1, startTimer1 },
		{ initTimer2, startTimer2 }, // 8 bit
#ifdef ARDUINO_MEGA
		{ initTimer3, startTimer3 },
		{ initTimer4, startTimer4 },
		{ initTimer5, startTimer5 },
#endif
	};

	switch (id) {
		case TimerId0:
			return timers + 0;
		case TimerId1:
			return timers + 1;
		case TimerId2:
			return timers + 2;
		case TimerId3:
			return timers + 3;
		case TimerId4:
			return timers + 4;
		case TimerId5:
			return timers + 5;
		default:
			return 0;
	}
}

static void printTimers(Buffer &send) {
}

#else
static const uint16_t DEFSLOP = 0;
static const uint16_t MAXSLOP = 0;
static const uint16_t MAXJITTER = 0;
static const uint8_t MAXLOOPS = 1;

typedef struct {
	void init() volatile {
	}

	void start(uint32_t us) volatile {
	}

	void stop() volatile {
	}
}FastTimer;

static FastTimer *getFastTimer(uint8_t id) {
	static FastTimer timer;
	return &timer;
}

static void printTimers(Buffer &send) {
}

#endif

class TimerInfo {
	enum {
		Count1,
		Count2,
		Count3,
		Count4,
		Count0,

		CountSleeps, // 5
		CountArrivals,
		CountLate,
		CountDec,

		CountJumps,
		CountLoops,
		CountSpurious,

		CountLong,

		HistMax,
	};

	FastTimer *timer;
	uint32_t last;
	uint32_t next;
	uint32_t jump;

	uint32_t cycle1;
	uint32_t asleep;

	int16_t minlate;
	int16_t maxlate;
	int16_t minawake;
	int16_t maxawake;

	uint16_t slop;
	uint8_t retest;
	uint8_t idx;
	uint8_t bits;

	uint16_t hist[HistMax];

	static const int16_t InvalidVal = 32000;

	inline void addHist(uint8_t h) volatile {
		if (h < HistMax && hist[h] + 1 != 0)
			hist[h]++;
	}

	inline void reset() volatile {
		minlate = InvalidVal;
		minawake = InvalidVal;

		maxlate = -InvalidVal;
		maxawake = -InvalidVal;
	}

	inline uint8_t getId() const volatile {
		switch (idx) {
			case TimerId0:
				return 0;
			case TimerId1:
				return 1;
			case TimerId2:
				return 2;
			case TimerId3:
				return 3;
			case TimerId4:
				return 4;
			case TimerId5:
				return 5;
			case TimerId6:
				return 6;
			case TimerId7:
				return 7;
			case TimerId8:
				return 8;
			default:
				return 255;
		}
	}

public:

	inline void stop() volatile {
		if (timer)
			timer->stop();
	}

	inline void init(uint8_t id) volatile {
		this->idx = id;

		if ((timer = getFastTimer(id)))
			timer->init();

		last = 0;
		next = 0;

		bits = 0;
		jump = 0;

		retest = 255;
		slop = DEFSLOP;

		reset();
		sleep(MicrosToTicks(4096));
	}

	inline uint32_t getJump(uint32_t now) volatile {
		if (jump && tdiff32(jump, now) > slop)
			return jump;

		jump = 0;

		return 0;
	}

	inline void sleep(uint32_t future) volatile {
		uint32_t now = clockTicks();
		int32_t ticks = tdiff32(future, now);
		ticks = max(0, ticks);

		next = now + ticks;

		if (ticks == 0)
			addHist(Count0);
		else if (ticks > MicrosToTicks(20000))
			addHist(CountLong);
		else {
			uint16_t idx = ticks >> 10; // 1024

			if (idx <= Count4)
				addHist(idx);
		}

#ifdef ARDUINO_MEGA
		if (ticks > MicrosToTicks(4096)) {
			addHist(CountJumps);
			ticks -= MicrosToTicks(2048); // jump till off the prescalers
			jump = next;
		} else {
			jump = 0;

			if (ticks >= slop)
			ticks -= slop;
			else
			ticks = 0;
		}
#else
		if (ticks >= slop)
			ticks -= slop;
		else
			ticks = 0;
#endif

		if (timer) {
#ifdef ARDUINO
			bits = getPrescaleBits(idx, ticks);
			timer->start(bits, ticks);
#else
			timer->start(ticks);
#endif
		}

		if (!jump) {
			uint32_t now = clockTicks();
			int32_t awake = tdiff32(now, last);
			minawake = min(minawake, awake);
			maxawake = max(maxawake, awake);
			addHist(CountSleeps);
		}

		last = now;
	}

	inline uint32_t awoke() volatile {
		uint32_t now = clockTicks();
		asleep = tdiff32(now, last);
		last = now;

		if (!jump) {
			addHist(CountArrivals);

			int32_t late = tdiff32(now, next);

			if (-late > slop) {
				addHist(CountSpurious);
			} else {
				static const int8_t EARLY = (TICKTOUS >> 2);
				minlate = min(minlate, late);
				maxlate = max(maxlate, late);

				if (late <= -EARLY) {
					addHist(CountLate);
					if (slop > 1)
						slop--;
					retest = 255;
				} else if (late >= EARLY) {
					if (!(--retest)) {
						addHist(CountDec);
						retest = 8;
						slop = max(slop + 1, 1);
					}
				} else {
					retest = 255;
				}
			}
		}

		return now;
	}

	void send(Buffer &send) volatile {
		send.p1(F("timer"));
		send.json(F("id"), getId());
		send.json(F("idx"), idx);
		send.jsonf(F("slop"), TicksToMicrosf(slop));

		if (minawake != InvalidVal) {
			send.json(F("-cycles"), minawake);
			send.json(F("+cycles"), maxawake);
			send.jsonf(F("-awake"), TicksToMicrosf(minawake));
			send.jsonf(F("+awake"), TicksToMicrosf(maxawake));
		}

		if (minlate != InvalidVal) {
			send.jsonf(F("-late"), TicksToMicrosf(minlate));
			send.jsonf(F("+late"), TicksToMicrosf(maxlate));
		}

		uint16_t count = hist[CountSleeps];
		uint16_t lates = hist[CountLate];

		if (lates && lates < count)
			send.jsonf(F("late"), 100.0f * lates / count);

		send.json(F("bits"), bits);
		send.json(F("jumping"), jump > 0);

		send.jsonf(F("sleep"), TicksToMicrosf(tdiff32(next, last)));
		send.jsonf(F("asleep"), TicksToMicrosf(asleep));

		MetricsHist::sendHist(send, F("counts"), hist, HistMax);
		send.p2();

		reset();
	}

	inline void addLoops() volatile {
		addHist(CountLoops);
	}
};

static volatile struct {
private:

	TimerInfo timers[MaxTimers];

public:

	inline volatile TimerInfo* getTimer(uint8_t id) volatile {
		assert(id >= 0 && id < MaxTimers);
		//assert(timers[id].id() == id);
		return timers + id;
	}

	void initTimers() volatile {
		for (uint8_t i = 0; i < MaxTimers; i++)
			timers[i].init(i);
	}

	void send(Buffer &send) volatile {
		for (uint8_t i = 0; i < MaxTimers; i++)
			timers[i].send(send);

		printTimers(send);
	}
} timers;

static void handleTimer(uint8_t id) {
	if (id < MaxTimers) {
		volatile TimerInfo *t = timers.getTimer(id);

		t->stop();

		uint32_t now = t->awoke();
		uint32_t next = t->getJump(now);

		if (!next)
			switch (id) {
#ifndef TIMER_TEST
				case 0:
#ifndef NOEFI
					next = now + encoder.run(now);
#endif
					break;
				case 1:
					for (uint8_t i = 0; i < MAXLOOPS; i++) {
						next = BitPlan::runEvents(now, 0, MAXJITTER);
						now = clockTicks();

						if (tdiff32(next, now) > MAXJITTER)
							break;
					}
					break;
#endif
				default:
					next = now + MicrosToTicks(4000);
			}

		t->sleep(next);
	}
}

void simTimerEncoder(uint32_t next) {
	volatile TimerInfo *t = timers.getTimer(0);
	t->awoke();
	t->sleep(next);
}

void simTimerEvents(uint32_t next) {
	volatile TimerInfo *t = timers.getTimer(1);
	t->awoke();
	t->sleep(next);
}

static void brokercb(Buffer &send, BrokerEvent &be, void *data) {
	timers.send(send);
}

void Timers::init() {
	timers.initTimers();

	if (0)
		handleTimer(TimerId1); // get rid of warning

	broker.add(brokercb, 0, F("timers"));
}

uint16_t Timers::mem(bool alloced) {
	return alloced ? 0 : sizeof(timers);
}

void Timers::sleep(uint32_t us) {
	/* no good without idle timers!  maybe wdt, more contention?
	 set_sleep_mode(SLEEP_MODE_IDLE);
	 PRR = PRR | 0b00100000;
	 sleep_enable();
	 sleep_mode();
	 PRR = PRR | 0b00100000;
	 sleep_disable();
	 */
}

