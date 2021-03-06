#define EXTERN

#ifdef STM32
#include "st_main.h"
#include "st_uart.h"
#include "st_adc.h"
#include "st_gpio.h"
#endif

#include "Hardware.h"
#include "Tasks.h"
#include "GPIO.h"
#include "System.h"

#ifndef NOEFI
#include "Encoder.h"
#include "Schedule.h"
#include "Strategy.h"
#include "Events.h"
#include "Timers.h"
#endif

#include "Codes.h"
#include "Epoch.h"

#include "Display.h"
#include "Vehicle.h"
#include "Bus.h"

#ifndef NDEBUG
#define MYNAME(x)	F(#x)
#else
#define MYNAME(x)	0
#endif

#include "efi_pins.h"

static void initPins(ZoneId zone) {
	for (uint8_t i = 0; i < MaxPins; i++) {
		GPIO::PinDef *pd = pins + i;
		pd->ext = getPinExt(zone, (PinId)i);
	}

	gpio.init(pins, MaxPins);
}

void initPins() {
	initPins(ZoneDash);
}

#ifdef ARDUINO
void Hardware::flush() {
	Buffer &send = hardware.send();
	Buffer &recv = hardware.recv();

	while (Serial.available())
		recv.myputch(Serial.read());

	char buf[16];
	size_t n = 0;

	if ((n = send.read(buf, sizeof(buf))) > 0)
		Serial.write(buf, n);
}

void GPIO::init(PinDef *pd) const {
}
#endif

#ifdef STM32

void toggleled(uint8_t id) {
#ifdef __STM32F4_DISCOVERY_H
	static Led_TypeDef leds[] = {
		LED3,
		LED4,
		LED5,
		LED6,
	};

	BSP_LED_Toggle(leds[id & 0x3]);
#endif
}

#define IRQNONE	((IRQn_Type)0)

static IRQn_Type getIRQ(PinId pt) {
	switch(pt) {
#ifdef STM32L1
		//return EXTI9_5_IRQn;
		//return EXTI4_IRQn;
		//case IN_BUTTON:
		//return EXTI15_10_IRQn;
#endif
		default:
		return IRQNONE;
	}
}

void GPIO::init(PinDef *pd) const {
	STGPIO::getPortName(pd);

	GPIO_InitTypeDef gpio = {0};
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;

	if (pd->mode & PinModeOutput) {
		gpio.Mode = GPIO_MODE_OUTPUT_PP;
		STGPIO::init(pd->getId(), &gpio);
	} else if (pd->mode & PinModeInput) {
		IRQn_Type irq = getIRQ(pd->getId());

		if (irq == IRQNONE) {
			gpio.Mode = GPIO_MODE_INPUT;
			STGPIO::init(pd->getId(), &gpio);
		} else {
		}
	}
}

void Hardware::flush() {
	for (uint8_t i = 0; i < nchannels(); i++) {
		if (i < ARRSIZE(uarts)) {
			if (uarts[i].isValid()) {
				uarts[i].send(send(i));
				uarts[i].recv(recv(i));
			} else
				ch[i].send.clear();
		}
	}
}

int main(void) {
	stmicro.init();

	initPins();

	//stled.setPin(OUT_LED2);

	if (!uarts[0].init(0, 3, PinShellRX, PinShellTX))
		myerror();

	//stled.toggle();
	//stadc.init();
	//stadc.start();
	//stled.setLoHi(100, 500);

	initSystem(true);

	while (true) {
		if (stbutton.isDown()) {
		}

		taskmgr.check();
	}
}
#else

void toggleled(uint8_t id) {
}

#ifdef linux

#include <stdio.h>
#include <time.h>
#include <unistd.h>

void Hardware::flush() {
	char buf[32];
	size_t n = 0;

	extern int available(int fd);

	while (available(0) > 0)
		if ((n = read(0, buf, 1)) > 0)
			hardware.recv(0).write(buf, n);

	while ((n = hardware.send(0).read(buf, sizeof(buf))) > 0) {
		n = fwrite(buf, 1, n, stdout);
		fflush(stdout);
	}
}

void GPIO::init(PinDef *pd) const {
}

static void stabilize(uint16_t ms) {
#ifndef NOEFI
	uint32_t start = clockTicks();

	while (clockTicks() - start < ms * 1000)
		BitSchedule::runSchedule(clockTicks(), 0);
#endif
}

static void runDecoderTest() {
#ifndef NOEFI
	BitPlan bp;

	bzero(&bp, sizeof(bp));

	for (uint32_t max = 10000; max <= 100000; max *= 10) {
		for (uint32_t ang = 0; ang <= 65536u; ang += 8192) {
			int32_t skip = max / 2;
			skip = 5000;

			printf("%10d %10u %10u", ang, max, skip);

			for (int32_t i = -skip; i <= skip; i += skip) {
				bp.setEvent(0, 0, 0, ang, i, 0);
				printf("%10d", bp.calcTicks(max));
			}

			printf("\n");
		}

		printf("\n");
	}
#endif
}

static void runTimeTest() {
}

#include "Stream.h"

int main(int argc, char **argv) {
	bool doperfect = false;
	bool efi = true;
	int opt;

	while ((opt = getopt(argc, argv, "edpt")) >= 0) {
		switch (opt) {
		case 'e':
			efi = !efi;
			break;
		case 'd':
			runDecoderTest();
			break;
		case 'p':
			doperfect = !doperfect;
			break;
		case 't':
			runTimeTest();
			break;
		default:
			fprintf(stderr, "Usage %s -[edpt]\n", argv[0]);
			exit(1);
		}
	}

	initPins();

	initSystem(efi);

#ifndef NOEFI
	if (false)
		for (int i = 0; i < 0; i++) {
			logFine("b %d %ld %d", i, time(0), clockTicks());
			delayTicks(1);
			logFine("a %d %ld %d", i, time(0), clockTicks());
			delayTicks(50 * 1000);
		}

	if (false)
		for (float i = 0; i <= 5; i += 0.125f) {
			setParamFloat(SensorMAF, i);
			stabilize(10);
			logFine("maf %10g %10g %10g %10g", i, getParamFloat(FuncMafTransfer), getParamFloat(CalcVolumetricRate), getParamFloat(TableInjectorTiming));
		}

	if (false)
		for (int i = getParamUnsigned(ConstIdleRPM) / 2; i <= 6000; i += 1500) {
			setParamFloat(SensorDEC, i);
			stabilize(10 + 60000 / i);
			logFine("final %10d %10d %10g %10g %10g %10g", i, getParamUnsigned(SensorRPM), getParamFloat(CalcFinalPulseAdvance), getParamFloat(CalcFinalSparkAdvance),
					getParamFloat(CalcFinalPulseWidth), getParamFloat(CalcFinalSparkWidth));
		}
#endif

	if (doperfect) {
		printf("Running perfect time!\n");
		extern uint32_t perfectus;
		perfectus = 1000;
	} else
		printf("Running..\n");

	while (true) {
		uint32_t us = taskmgr.check();

		if (us > 0)
			delayus(us);
	}
}

#endif
#endif
