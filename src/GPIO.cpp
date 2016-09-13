#include "GPIO.h"

#ifndef NDEBUG
#define MYNAME(x)	x, #x
#else
#define MYNAME(mode, x, pin)	x, 0, 0
#endif

#include "efi_pins.h"

#ifdef STM32
#include "stm32_main.h"

enum {
	UNK,
	PA,
	PB,
	PC,
	PD,
	PH
};

#define IRQNONE	((IRQn_Type)0)

typedef struct {
	uint8_t port:4;
	uint8_t bit:4;

	GPIO_TypeDef *getPort() const {
		switch (port) {
		case PA:
			return GPIOA;
		case PB:
			return GPIOB;
		case PC:
			return GPIOC;
		case PD:
			return GPIOD;
		case PH:
			return GPIOH;
		}

		return 0;
	}

	void enable() const {
		switch (port) {
		case PA:
			__HAL_RCC_GPIOA_CLK_ENABLE()
			;
			break;
		case PB:
			__HAL_RCC_GPIOB_CLK_ENABLE()
			;
			break;
		case PC:
			__HAL_RCC_GPIOC_CLK_ENABLE()
			;
			break;
		case PD:
			__HAL_RCC_GPIOD_CLK_ENABLE()
			;
			break;
		case PH:
			__HAL_RCC_GPIOH_CLK_ENABLE()
			;
			break;
		}
	}

	uint16_t getMask() const {
		return 1U << bit;
	}

	char getPortChar() const {
		if (port == UNK)
			return 'U';
		if (port == PH)
			return 'H';
		return 'A' + (port - PA);
	}
} STMPIN;

static const STMPIN stpins[65] = {
	{ UNK },
	{ UNK },
	{ PC, 13 },
	{ PC, 14 },
	{ PC, 15 },
	{ PH, 0 },
	{ PH, 1 },
	{ UNK },
	{ PC, 0 },
	{ PC, 1 },
	{ PC, 2 },
	{ PC, 3 },
	{ 0, 0 },
	{ 0, 0 },
	{ PA, 0 },
	{ PA, 1 },
	{ PA, 2 },
	{ PA, 3 },
	{ UNK },
	{ UNK },
	{ PA, 4 },
	{ PA, 5 },
	{ PA, 6 },
	{ PA, 7 },
	{ PC, 4 },
	{ PC, 5 },
	{ PB, 0 },
	{ PB, 1 },
	{ PB, 2 },
	{ PB, 10 },
	{ PB, 11 },
	{ UNK },
	{ UNK },
	{ PB, 12 },
	{ PB, 13 },
	{ PB, 14 },
	{ PB, 15 },
	{ PC, 6 },
	{ PC, 7 },
	{ PC, 8 },
	{ PC, 9 },
	{ PA, 8 },
	{ PA, 9 },
	{ PA, 10 },
	{ PA, 11 },
	{ PA, 12 },
	{ PA, 13 },
	{ UNK },
	{ UNK },
	{ PA, 14 },
	{ PA, 15 },
	{ PC, 10 },
	{ PC, 11 },
	{ PC, 12 },
	{ PD, 2 },
	{ PB, 3 },
	{ PB, 4 },
	{ PB, 5 },
	{ PB, 6 },
	{ PB, 7 },
	{ UNK },
	{ PB, 8 },
	{ PB, 9 },
	{ UNK },
	{ UNK },
};


static void getPortName(char *buf, uint8_t ext) {
	int pin = stpins[ext].bit;
	char ch = stpins[ext].getPortChar();
	mysprintf(buf, "P%c%d", ch, pin);
}

static GPIO_TypeDef *getPort(const GPIO::PinDef *pd) {
	return stpins[pd->ext].getPort();
}

static uint16_t getPin(const GPIO::PinDef *pd) {
	return stpins[pd->ext].getMask();
}

static IRQn_Type getIRQ(GPIO::PinId pt) {
	switch(pt) {
	default:
		return IRQNONE;
	}
}

#endif

const GPIO::PinDef *GPIO::getPinDef(PinId id) {
	return pins + id;
}

bool GPIO::isPinSet(PinId id) {
#ifdef STM32
	uint16_t last = ::HAL_GPIO_ReadPin(getPort(pins + id), getPin(pins + id));
#elif ARDUINO
	uint16_t last = ::digitalRead(pins[id].pin);
#else
	uint16_t last = pins[id].last;
#endif

	if (pins[id].last != last) {
		pins[id].last = last;
		pins[id].changed = millis();
	}

	return pins[id].last;
}

void GPIO::setPin(PinId id, bool v) {
	if (pins[id].last != v) {
		pins[id].last = v;
		pins[id].changed = millis();
	}

#ifdef STM32
	::HAL_GPIO_WritePin(getPort(pins + id), getPin(pins + id), v ? GPIO_PIN_SET : GPIO_PIN_RESET);
#elif ARDUINO
	::digitalWrite(pins[id].pin, v);
#endif
}

uint16_t GPIO::analogRead(PinId id) {
#ifdef ARDUINO
	uint16_t last = ::analogRead(pins[id].pin);
#else
	uint16_t last = pins[id].last;
#endif

	if (pins[id].last != last) {
		pins[id].last = last;
		pins[id].changed = millis();
	}

	return pins[id].last;
}

void GPIO::analogWrite(PinId id, uint16_t v) {
	if (pins[id].last != v) {
		pins[id].last = v;
		pins[id].changed = millis();
	}

#ifdef ARDUINO
	::analogWrite(pins[id].pin, v);
#endif
}

static void deinit(GPIO::PinId id) {
#ifdef STM32
	GPIO::PinDef *pd = pins + id;
	const STMPIN *st = stpins + pd->ext;
	IRQn_Type irq = getIRQ(id);

	if (irq != IRQNONE)
		HAL_NVIC_DisableIRQ(irq);

	if (pd->mode & GPIO::PinModeOutput)
		GPIO::setPin(id, 0);

	GPIO_InitTypeDef gpio = { 0 };
	gpio.Pin = st->getMask();
	HAL_GPIO_DeInit(getPort(pd), gpio.Pin);
#endif
}

void GPIO::init() {
	for (uint8_t i = 0; i < MaxPins; i++) {
		PinDef *pd = pins + i;

		if (pd->ext > 0) {
#ifdef STM32
			const STMPIN *st = stpins + pd->ext;
			getPortName(pd->info, pd->ext);

			GPIO_InitTypeDef gpio = { 0 };
			gpio.Pin = st->getMask();
			gpio.Pull = GPIO_NOPULL;
			gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;

			if (pd->mode & PinModeOutput) {
				gpio.Mode = GPIO_MODE_OUTPUT_PP;
			} else if (pd->mode & PinModeInput) {
				IRQn_Type irq = getIRQ(pd->getId());

				if (irq == IRQNONE) {
					gpio.Mode = GPIO_MODE_INPUT;
				}

				/*
				if (id == GPIO::IN_RF_RX) {
					gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
					gpio.Speed = GPIO_SPEED_FREQ_HIGH;
					gpio.Pull = GPIO_NOPULL;
					gpio.Alternate = GPIO_AF1_TIM2;
					HAL_GPIO_Init(getPort(), &gpio);
					global.irq_enable(irq, 0, 0);
				} else if (id == GPIO::IN_BUTTON) {
					gpio.Mode = GPIO_MODE_IT_FALLING;
					HAL_GPIO_Init(getPort(), &gpio);
					global.irq_enable(irq, 0x0f, 0);
				}
				*/
			}

			HAL_GPIO_Init(st->getPort(), &gpio);
			st->enable();
#elif ARDUINO
			if (pd->mode & PinModeOutput)
				pinMode(def.pin, OUTPUT);
			else if (pd->mode & PinModeInput)
				pinMode(def.pin, INPUT_PULLUP);
#endif
		}
	}
}
