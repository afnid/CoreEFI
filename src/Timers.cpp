// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Decoder.h"
#include "Encoder.h"
#include "Timers.h"
#include "Events.h"

#ifdef STM32
//#define TIMER_TEST
#endif

// Timers 0-3 valid on uno, 0, 2 are 8 bit
// Timers 0-5 valid on mega, 0, 2 are 8 bit
// Timers 0-8 valid on due, all 32 bit

enum {
	TimerId4,
	TimerId5,

	// first too are always used unless TIMER_TEST is defined
	MaxTimers, // after here are disabled..

	TimerId1,
	TimerId2,
	TimerId3,

	TimerId6,
	TimerId7,
	TimerId0,
	TimerId8, // TODO: do not use, locked up?
};

static void handleTimer(uint8_t id);

#ifdef STM32

static const uint8_t  DEFSLOP = 5;
static const uint8_t  MAXSLOP = 100;
static const uint8_t MAXJITTER = 10;
static const uint8_t MAXLOOPS = 4;

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_tim.h"

extern "C" void ErrorHandler(void);

static int stprio = 0;

class FastTimer {
public:
	TIM_HandleTypeDef handle;
	TIM_TypeDef *tim;
	volatile uint8_t bits;
	volatile uint8_t clock;
	volatile uint8_t apb;
	volatile uint8_t irq;
	volatile uint32_t mask;
	volatile uint16_t ticks;
	volatile uint16_t div;
	volatile uint8_t state;

	void start(int32_t arr) {
		arr = max(arr, 2) - 1;

		__HAL_TIM_SET_AUTORELOAD(&handle, arr);

		if (HAL_TIM_Base_Start_IT(&handle) != HAL_OK)
			ErrorHandler();
	}

	void stop() {
		// enbable/disable work, but can't write arr register without this..

		if (HAL_TIM_Base_Stop_IT(&handle) != HAL_OK)
			ErrorHandler();
	}

	void init() {
		if (!state) {
			div = getFreq() / 1000L / 1000L;
			myzero(&handle, sizeof(handle));
			handle.Instance = tim;

			handle.Init.Period = 1000000L - 1;
			handle.Init.Prescaler = div - 1;
			handle.Init.CounterMode = TIM_COUNTERMODE_UP;

			if (HAL_TIM_Base_Init(&handle) != HAL_OK)
				ErrorHandler();

			if (HAL_TIM_Base_Start_IT(&handle) != HAL_OK)
				ErrorHandler();
		}
	}

	bool isSame(TIM_HandleTypeDef *ptr) const volatile {
		return ptr == &handle;
	}

	void notifyHal() {
		ticks++;

		if (handle.Instance)
			HAL_TIM_IRQHandler(&handle);
	}

	int isEnabled() volatile {
		if (apb == 1)
			return (RCC->APB1ENR & mask) != 0;
		return (RCC->APB2ENR & mask) != 0;
	}

	void mspinit() volatile {
		state++;
		enable();

		if (irq) {
			HAL_NVIC_SetPriority((IRQn_Type)irq, stprio, stprio);
			HAL_NVIC_EnableIRQ((IRQn_Type)irq);
			stprio++;
		}
	}

	void enable() volatile {
		state++;
		tim->CNT = 0;

		if (apb == 1)
			RCC->APB1ENR |= mask;
		else
			RCC->APB2ENR |= mask;
	}

	void disable() volatile {
		state = 3;
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

	void send(uint8_t i) {
		channel.p1(F("tim"));
		channel.send("i", i);
		channel.send("id", ++i);
		channel.send("state", state);
		channel.send("bits", bits);
		channel.send("clk", clock);
		channel.send("apb", apb);
		channel.send("en", (uint8_t)isEnabled());
		channel.send("rq", irq);
		channel.send("ticks", ticks);
		channel.send("per", handle.Init.Period);
		channel.send("arr", tim->ARR);
		channel.send("psc", tim->PSC);
		channel.send("cnt", tim->CNT);
		channel.p2();
		channel.nl();
	}
};

static FastTimer stm32timers[] = {
		{ {}, TIM1, 16, 0, 2, 0, RCC_APB2ENR_TIM1EN, 0, 0, 0 },
		{ {}, TIM2, 32, 2, 1, TIM2_IRQn, RCC_APB1ENR_TIM2EN, 0, 0, 0 },
		{ {}, TIM3, 16, 2, 1, TIM3_IRQn, RCC_APB1ENR_TIM3EN, 0, 0, 0 },
		{ {}, TIM4, 16, 2, 1, TIM4_IRQn, RCC_APB1ENR_TIM4EN, 0, 0, 0 },
		{ {}, TIM5, 32, 2, 1, TIM5_IRQn, RCC_APB1ENR_TIM5EN, 0, 0, 0 },
		{ {}, TIM6, 16, 2, 1, 0, RCC_APB1ENR_TIM6EN, 0, 0, 0 },
		{ {}, TIM7, 16, 2, 1, TIM7_IRQn, RCC_APB1ENR_TIM7EN, 0, 0, 0 },
		{ {}, TIM8, 16, 0, 2, 0, RCC_APB2ENR_TIM8EN, 0, 0, 0 },
		{ {}, TIM9, 16, 0, 2, 0, RCC_APB2ENR_TIM9EN, 0, 0, 0 },
		{ {}, TIM10, 16, 0, 2, 0, RCC_APB2ENR_TIM10EN, 0, 0, 0 },
		{ {}, TIM11, 16, 0, 2, 0, RCC_APB2ENR_TIM11EN, 0, 0, 0 },
		{ {}, TIM12, 16, 2, 1, 0, RCC_APB1ENR_TIM12EN, 0, 0, 0 },
		{ {}, TIM13, 16, 2, 1, 0, RCC_APB1ENR_TIM13EN, 0, 0, 0 },
		{ {}, TIM14, 16, 2, 1, 0, RCC_APB1ENR_TIM14EN, 0, 0, 0 }
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

#define myHandler(idx, id, led)	extern "C" void TIM##id##_IRQHandler() {\
		stm32timers[idx].notifyHal();\
		if (TimerId##id != TimerId2)\
			handleTimer(TimerId##id);\
}

myHandler(0, 1, 0)
myHandler(1, 2, 0)
//myHandler(2, 3, 0)
myHandler(3, 4, 0)
myHandler(4, 5, 0)
//myHandler(5, 6, 0)
//myHandler(6, 7, 0)
//myHandler(7, 8, 0)
//myHandler(8, 9, 0)
//myHandler(9, 10, 0)
//myHandler(10, 11, 0)
//myHandler(11, 12, 0)
//myHandler(12, 13, 0)
//myHandler(13, 14, 0)
//myHandler(14, 15, 0)

int getTimerId(TIM_TypeDef *tim) {
	for (int i = 0; i < ntimers; i++)
		if (stm32timers[i].tim == tim)
			return i;
	return -1;
}

int getTimerId(TIM_HandleTypeDef *handle) {
	for (int i = 0; i < ntimers; i++)
		if (stm32timers[i].isSame(handle))
			return i;
	return -1;
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim) {
	int id = getTimerId(htim);

	if (id >= 0)
		stm32timers[id].mspinit();
}

void initTimers2() {
	for (int i = 0; i < ntimers; i++)
		if (i != 2 && i != 5)
			stm32timers[i].init();
}

void startTimers() {
	for (int i = 0; i < ntimers; i++)
		if (i != 2 && i != 5)
			stm32timers[i].enable();
}

void stopTimers() {
	for (int i = 0; i < ntimers; i++)
		if (i != 2 && i != 5)
			stm32timers[i].disable();
}

void printTimers() {
	float div = 1000 * 1000;

	channel.p1("clocks");
	channel.send("hclk", HAL_RCC_GetHCLKFreq() / div);
	channel.send("pclk1", HAL_RCC_GetPCLK1Freq() / div);
	channel.send("pclk2",  HAL_RCC_GetPCLK2Freq() / div);
	channel.p2();
	channel.nl();

	for (int i = 0; i < ntimers; i++)
		stm32timers[i].send(i);
}

void initTimer(TIM_TypeDef *tim) {
	uint8_t id = getTimerId(tim);

	if (id > 0)
		stm32timers[id].init();
}

#elif defined(ARDUINO_DUE)
#include <Arduino.h>
static const uint8_t DEFSLOP = MicrosToTicks(6);
static const uint8_t MAXSLOP = MicrosToTicks(20);
static const uint8_t MAXJITTER = MicrosToTicks(15);
static const uint8_t MAXLOOPS = 4;

#if 0
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

#else

#define myHandler2(id, tc, c)	void TC##id##_Handler() { TC_GetStatus(TC##tc, c); handleTimer(TimerId##id); }

myHandler2(0, 0, 0)
myHandler2(1, 0, 1)
myHandler2(2, 0, 2)
myHandler2(3, 1, 0)
myHandler2(4, 1, 1)
myHandler2(5, 1, 2)
myHandler2(6, 2, 0)
myHandler2(7, 2, 1)
myHandler2(8, 2, 2)

typedef struct {
	Tc *tc;
	uint32_t channel;
	IRQn_Type irq;

	static const uint32_t ratio = VARIANT_MCK / 1000 / 1000 / 2;

	void init() volatile {
        pmc_set_writeprotect(false);
        pmc_enable_periph_clk((uint32_t)irq);
        TC_Configure(tc, channel, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK1);
        tc->TC_CHANNEL[channel].TC_IER = TC_IER_CPCS;
        tc->TC_CHANNEL[channel].TC_IDR = ~TC_IER_CPCS;
	}

	inline void start(uint32_t us) volatile {
    	NVIC_ClearPendingIRQ(irq);
		us = max(us, 1);
        TC_SetRC(tc, channel, ratio * us);
		TC_Start(tc, channel);
    	NVIC_EnableIRQ(irq);
	}

	inline void stop() volatile {
		NVIC_DisableIRQ(irq);
		TC_Stop(tc, channel);
	}
} FastTimer;

static FastTimer *getFastTimer(uint8_t id) {
	static FastTimer timers[] = {
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
		case TimerId6:
			return timers + 6;
		case TimerId7:
			return timers + 7;
		case TimerId8:
			return timers + 8;
		default:
			return 0;
	}
}

#endif

#elif ARDUINO // MEGA
#include <Arduino.h>
// TODO: had < 2% error up to 2k rpm, but broke it when i optimized for due and added the additional calls for current time
static const uint8_t DEFSLOP = 90;
static const uint8_t MAXSLOP = 255;
static const uint8_t MAXJITTER = 50;
static const uint8_t MAXLOOPS = 1;

static inline uint8_t getPrescaleBits(uint8_t id, int32_t &us) {
	static const uint16_t TIME_RATIO = F_CPU / 1000 / 1000; // 16
	static const uint16_t max16 = 65535u;
	static const uint16_t max8 = 255;

	uint16_t max = id == TimerId2 || id == TimerId0 ? max8 : max16;
	us = max(us, 1); // TODO: bump this up for lockups
	us = us * TIME_RATIO - 1;

	if (us <= max) // heavily modified/fixed from timer1/timer3
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
oneShotTimer(3);
oneShotTimer(4);
oneShotTimer(5);

static FastTimer *getFastTimer(uint8_t id) {
	static FastTimer timers[] = {
			{ initTimer0, startTimer0 }, // 8 bit
			{ initTimer1, startTimer1 },
			{ initTimer2, startTimer2 }, // 8 bit
			{ initTimer3, startTimer3 },
			{ initTimer4, startTimer4 },
			{ initTimer5, startTimer5 },
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

#else
static const uint8_t  DEFSLOP = 0;
static const uint8_t  MAXSLOP = 0;
static const uint8_t MAXJITTER = 0;
static const uint8_t MAXLOOPS = 1;

typedef struct {
	void init() volatile {
	}

	void start(uint32_t us) volatile {
	}

	void stop() volatile {
	}
} FastTimer;

static FastTimer *getFastTimer(uint8_t id) {
	static FastTimer timer;
	return &timer;
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
		CountLates,

		CountJumps, // 8
		CountLoops,
		CountSpurious,

		CountLong, // 11

		HistMax,
	};

	FastTimer *timer;
	uint32_t last;
	uint32_t next;
	uint32_t jump;

	uint32_t asleep;

	int16_t minlate;
	int16_t maxlate;
	int16_t minawake;
	int16_t maxawake;

	uint8_t slop;
	uint8_t idx;
	uint8_t bits;
	uint8_t retest;

	uint16_t hist[HistMax];

	static const int16_t InvalidVal = 32767;

	inline void addHist(uint8_t h) volatile {
		if (h < HistMax && hist[h] + 1 != 0)
			hist[h]++;
	}

	inline void reset() volatile {
		minlate = InvalidVal;
		maxlate = -minlate;
		minawake = InvalidVal;
		maxawake = -minawake;
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

	inline void init(uint8_t id, uint16_t us) volatile {
		this->idx = id;

		if ((timer = getFastTimer(id)))
			timer->init();

		last = 0;
		next = 0;

		bits = 0;
		retest = 0;
		jump = 0;

		slop = DEFSLOP;

		reset();
		sleep(0);
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

		if (!jump) {
			int32_t awake = tdiff32(now, last);
			minawake = min(minawake, awake);
			maxawake = max(maxawake, awake);
			addHist(CountSleeps);
		}

		last = now;
		next = now + ticks;

		if (ticks == 0)
			addHist(Count0);
		else if (ticks > 20000)
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

		ticks = TicksToMicros(ticks);

		if (timer) {
#ifdef ARDUINO_MEGA
			bits = getPrescaleBits(idx, ticks);
			timer->start(bits, ticks);
#else
			timer->start(ticks);
#endif
		}
	}

	inline uint32_t awoke() volatile {
		uint32_t now = clockTicks();
		asleep = tdiff32(now, last);

		if (!jump) {
			addHist(CountArrivals);

			int32_t late = tdiff32(now, next);

			if (late) {
				if (-late > slop) {
					addHist(CountSpurious);
				} else {
					minlate = min(minlate, late);
					maxlate = max(maxlate, late);

					if (late > 0) {
						slop = min(slop + 1, MAXSLOP);
						retest = 255;
						addHist(CountLates);
					} else if (late < -1 && slop > 1) { // consistently early
						if (!retest) {
							retest = 64; // reward good behavior
							slop--;
						} else
							retest--;
					} else
						retest = 255;
				}
			} else { // try to be a little early
				slop = min(slop + 1, MAXSLOP);
				retest = 255;
			}
		}

		last = now;

		return now;
	}

	void send() volatile {
		channel.p1(F("timer"));
		channel.send(F("id"), getId());
		channel.send(F("idx"), idx);
		channel.send(F("slop"), slop);

		channel.send(F("sleep"), tdiff32(next, last), false);
		channel.send(F("asleep"), asleep, false);

		if (minawake != InvalidVal) {
			channel.send(F("-awake"), minawake, false);
			channel.send(F("+awake"), maxawake, false);
		}

		if (minlate != InvalidVal) {
			channel.send(F("-late"), minlate, false);
			channel.send(F("+late"), maxlate, false);
		}

		channel.send(F("retest"), retest, false);

		uint16_t count = hist[CountSleeps];
		uint16_t lates = hist[CountLates];

		if (lates && lates < count)
			channel.send(F("late"), 100.0f * lates / count, false);

		channel.send(F("bits"), bits, false);
		channel.send(F("jumping"), jump > 0, false);

		sendHist(hist, HistMax);

		channel.p2();
		channel.nl();

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
			timers[i].init(i, MicrosToTicks(4096));
	}

	void send() volatile {
		for (uint8_t i = 0; i < MaxTimers; i++)
			timers[i].send();
	}
} timers;

static void handleTimer(uint8_t id) {
	volatile TimerInfo *t = timers.getTimer(id);

	t->stop();

	uint32_t now = t->awoke();
	uint32_t next = t->getJump(now);

	if (!next)
		switch (id) {
#ifndef TIMER_TEST
			case 0:
				next = now + encoder.run(now);
				break;
			case 1:
				for (uint8_t i = 0; i < MAXLOOPS; i++) {
					next = runEvents(now, 0, MAXJITTER);
					now = clockTicks();

					if (tdiff32(next, now) > MAXJITTER)
						break;
				}
				break;
#endif
			default:
				next = now + MicrosToTicks(200);
		}

	t->sleep(next);
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

uint16_t initTimers() {
	timers.initTimers();

	if (0)
		handleTimer(TimerId1); // get rid of warning

	return sizeof(timers);
}

void sendTimers() {
	timers.send();
}

void idleSleep(uint32_t us) {
	/* no good without idle timers!  maybe wdt, more contention?
	 set_sleep_mode(SLEEP_MODE_IDLE);
	 PRR = PRR | 0b00100000;
	 sleep_enable();
	 sleep_mode();
	 PRR = PRR | 0b00100000;
	 sleep_disable();
	 */
}

