#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Decoder.h"
#include "Encoder.h"
#include "Timers.h"
#include "Events.h"

// 6us steady slop on due with 4 timers
// #define TIMER_TEST
// Timers 1-5 are valid on mega
// Timers 0-8 are valid on due

enum {
	TimerId4,
	TimerId5,
	MaxTimers, // after here are disabled..

	TimerId0,
	TimerId1,
	TimerId2,
	TimerId3,
	TimerId6,
	TimerId7,
	TimerId8, // TODO: do not use, locked up?
};

static void handleTimer(uint8_t id);

#ifdef ARDUINO_DUE
#include <Arduino.h>
static const uint16_t DEFSLOP = MicrosToTicks(6);
static const uint16_t MAXSLOP = MicrosToTicks(6);
static const uint16_t MAXDELAY = MicrosToTicks(10);

#if 0
// too much floating point, slop at 122 us with 4 timers, mine is 5 us, DueTimer could be written way more efficiently, but I need every us!
#include "DueTimer.h"

#define myHandler(id)	static void handleTimer##id() { handleTimer(TimerId##id); }

myHandler(1)
myHandler(2)
myHandler(3)
myHandler(4)
myHandler(5)
myHandler(6)
myHandler(7)
myHandler(8)

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

static volatile FastTimer *getFastTimer(uint8_t id) {
	static volatile FastTimer timers[] = {
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

#elif ARDUINO
#include <Arduino.h>
static const uint16_t DEFSLOP = 128;
static const uint16_t MAXSLOP = 256;
static const uint16_t MAXDELAY = 50;

#define myHandler(id)	ISR(TIMER##id##_COMPA_vect) { handleTimer(TimerId##id); }

myHandler(1)
myHandler(2)
myHandler(3)
myHandler(4)
myHandler(5)

static uint8_t mask(uint8_t bits, uint8_t b0, uint8_t b1, uint8_t b2) {
	switch (bits) {
		case 0:
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

static void initTimer1(uint8_t bits, uint32_t us) {
#ifdef TIMSK1
	if (us > 0) {
		TCCR1A = 0;
		TCCR1B = bit(WGM12) | mask(bits, CS10, CS11, CS12);
		OCR1A = us;
		TIMSK1 = bit(OCIE1A); // interrupt on Compare A Match
	} else {
		TIMSK1 = 0;
		TCCR1B = 0;
		TCNT1 = 0;
	}
#endif
}

static void initTimer2(uint8_t bits, uint32_t us) {
#ifdef TIMSK2
	if (us > 0) { // TODO: 8 bit mess, no worky!
		TCCR2A = 0;
		TCCR2B = bit(WGM22) | bit(CS21);// | bit(CS20);
		OCR2A = 255 - 1;
		TIMSK2 = bit(OCIE2A);
	} else {
		TIMSK2 = 0;
		TCCR2B = 0;
		TCNT2 = 0;
	}
#endif
}

static void initTimer3(uint8_t bits, uint32_t us) {
#ifdef TIMSK3
	if (us > 0) {
		TCCR3A = 0;
		TCCR3B = bit(WGM32) | mask(bits, CS30, CS31, CS32);
		OCR3A = us;
		TIMSK3 = bit(OCIE3A);
	} else {
		TIMSK3 = 0;
		TCCR3B = 0;
		TCNT3 = 0;
	}
#endif
}

static void initTimer4(uint8_t bits, uint32_t us) {
#ifdef TIMSK4
	if (us > 0) {
		TCCR4A = 0;
		TCCR4B = bit(WGM42) | mask(bits, CS40, CS41, CS42);
		OCR4A = us;
		TIMSK4 = bit(OCIE4A);
	} else {
		TIMSK4 = 0;
		TCCR4B = 0;
		TCNT4 = 0;
	}
#endif
}

static void initTimer5(uint8_t bits, uint32_t us) {
#ifdef TIMSK5
	if (us > 0) {
		TCCR5A = 0;
		TCCR5B = bit(WGM52) | mask(bits, CS50, CS51, CS52);
		OCR5A = us;
		TIMSK5 = bit(OCIE5A);
	} else {
		TIMSK5 = 0;
		TCCR5B = 0;
		TCNT5 = 0;
	}
#endif
}

static uint8_t getPrescaleBits(int32_t &us) {
	static const uint16_t TIME_RATIO = F_CPU / 1000 / 1000; // 16
	static const uint16_t max = 65535u;

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

typedef struct {
	uint8_t id;
	void (*initTimer)(uint8_t bits, uint32_t us);

	inline uint16_t getTicks() volatile {
		switch (id) {
#ifdef TCNT1
			case TimerId1:
				return TCNT1;
#endif
#ifdef TCNT2
			case TimerId2:
				return TCNT2;
#endif
#ifdef TCNT3
			case TimerId3:
				return TCNT3;
#endif
#ifdef TCNT4
			case TimerId4:
				return TCNT4;
#endif
#ifdef TCNT5
			case TimerId5:
				return TCNT5;
#endif
		}

		return 0;
	}

	void init() volatile {
	}

	inline void start(uint8_t bits, uint32_t us) volatile {
		this->initTimer(bits, us);
	}

	inline void stop() volatile {
		cli(); // solves hang with short shorter timeouts
		start(0, 0);
		sei();
	}
} FastTimer;

static volatile FastTimer *getFastTimer(uint8_t id) {
	static volatile FastTimer timers[] = {
			{ TimerId1, initTimer1 },
			{ TimerId2, initTimer2 },
			{ TimerId3, initTimer3 },
			{ TimerId4, initTimer4 },
			{ TimerId5, initTimer5 },
	};

	switch (id) {
		case TimerId1:
			return timers + 0;
		case TimerId2:
			return timers + 1;
		case TimerId3:
			return timers + 2;
		case TimerId4:
			return timers + 3;
		case TimerId5:
			return timers + 4;
		default:
			return 0;
	}
}

#else
static const uint16_t  DEFSLOP = 0;
static const uint16_t  MAXSLOP = 0;
static const uint16_t  MAXDELAY = 0;

typedef struct {
	void init() volatile {
	}

	void start(uint32_t us) volatile {
	}

	void stop() volatile {
	}
} FastTimer;

static volatile FastTimer *getFastTimer(uint8_t id) {
	static volatile FastTimer timer;
	return &timer;
}

#endif

class TimerInfo {
	volatile FastTimer *timer;
	uint32_t last;
	uint32_t next;
	int32_t jump;

	uint32_t asleep;

	uint16_t count;
	uint16_t lates;

	uint16_t slop;
	int32_t minlate;
	int32_t maxlate;
	int16_t minawake;
	int16_t maxawake;

	uint8_t idx;
	uint8_t bits;
	uint8_t retest;

	static const int16_t InvalidVal = 32767;

	inline void reset() volatile {
		minlate = InvalidVal;
		maxlate = -InvalidVal;
		minawake = InvalidVal;
		maxawake = -InvalidVal;
	}

	inline uint8_t getId() const volatile {
		switch (idx) {
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
		}

		return 0;
	}

public:

	inline void stop() volatile {
		timer->stop();
	}

	inline void init(uint8_t id, uint16_t us) volatile {
		this->idx = id;

		timer = getFastTimer(id);
		timer->init();

		last = 0;
		next = 0;

		bits = 0;
		retest = 0;
		jump = 0;

		slop = DEFSLOP;

		reset();
		sleep(us);
	}

	inline uint32_t getJump(uint32_t now) volatile {
		if (jump) {
			int32_t ticks = tdiff32(jump, now);

			if (ticks > slop) {
				jump = ticks;
				return jump;
			}

			jump = 0;
		}

		return 0;
	}

	inline void sleep(int32_t us) volatile {
		uint32_t now = clock_ticks();
		int32_t awake = tdiff32(now, last);
		minawake = min(minawake, awake);
		maxawake = max(maxawake, awake);
		last = now;

#ifndef ARDUINO_DUE
		if (us > 4096) {
			jump = last + us;
			us -= 2048; // jump till off the prescalers
		} else {
			jump = 0;
		}
#endif

		next = now + us;

		if (us >= slop)
			us -= slop;
		else
			us = 0;

		us = TicksToMicros(us);

#ifdef ARDUINO
#ifdef ARDUINO_MEGA
		bits = getPrescaleBits(us);
		timer->start(bits, us);
#else
		timer->start(us);
#endif
#else
		timer->start(us);
#endif
	}

	inline uint32_t awoke() volatile {
		uint32_t now = clock_ticks();
		asleep = tdiff32(now, last);
		last = now;
		count++;

		if (!jump) {
			int16_t late = tdiff32(now, next);

			if (late) {
				minlate = min(minlate, late);
				maxlate = max(maxlate, late);

				if (late > 0) {
					slop = min(slop + 1, MAXSLOP);
					retest = 255;
					lates++;
				} else if (late < -1 && slop > 1) { // need to be consistently early
					if (!retest) {
						retest = 64; // reward for good behavior
						slop--;
					} else
						retest--;
				} else
					retest = 255;
			} else { // try to be at least 1 us early
				slop = min(slop + 1, MAXSLOP);
				retest = 255;
			}
		}

		return now;
	}

	inline uint16_t ticks() volatile {
		return tdiff32(next, last);
	}

	void send() volatile {
		channel.p1(F("timer"));
		channel.send(F("id"), getId());
		channel.send(F("idx"), idx);
		channel.send(F("slop"), slop);

		channel.send(F("count"), count, false);
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

		channel.send(F("bits"), bits, false);
		channel.send(F("jumping"), jump > 0, false);
		channel.send(F("retest"), retest, false);

		channel.send(F("lates"), lates, false);

		if (lates && lates < count)
			channel.send(F("late"), 100.0 * lates / count, false);

		channel.p2();
		channel.nl();

		reset();
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
			timers[i].init(i, 4096);
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
	uint32_t sleep = t->getJump(now);

	if (!sleep)
		switch (id) {
#ifndef TIMER_TEST
			case TimerId5:
				for (uint8_t i = 0; (sleep = runEvents(now, MAXDELAY)) <= MAXDELAY && i < 4; i++)
					now = clock_ticks();
				break;
			case TimerId4:
				sleep = encoder.run(now);
				break;
#endif
			default:
				sleep = 1000;
		}

	t->sleep(sleep);
}

void simTimerEncoder(uint32_t ticks) {
	volatile TimerInfo *t = timers.getTimer(TimerId4);
	t->awoke();
	t->sleep(ticks);
}

void simTimerEvents(uint32_t ticks) {
	volatile TimerInfo *t = timers.getTimer(TimerId5);
	t->awoke();
	t->sleep(ticks);
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
	/* no good without idle timers!  maybe wdt?
	 set_sleep_mode(SLEEP_MODE_IDLE);
	 PRR = PRR | 0b00100000;
	 sleep_enable();
	 sleep_mode();
	 PRR = PRR | 0b00100000;
	 sleep_disable();
	 */
}

