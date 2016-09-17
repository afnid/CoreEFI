#include "GPIO.h"

#ifndef NDEBUG
#define MYNAME(x)	x, F(#x)
#else
#define MYNAME(x)	x, 0
#endif

#include "efi_pins.h"

#ifdef STM32
#include "st_main.h"
#include "st_gpio.h"

#define IRQNONE	((IRQn_Type)0)

static GPIO_TypeDef *getPort(const GPIO::PinDef *pd) {
	return STGPIO::getPort(pd->ext);
}

static uint16_t getPin(const GPIO::PinDef *pd) {
	return STGPIO::getPin(pd->ext);
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
	uint16_t last = ::digitalRead(pins[id].ext);
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
	::digitalWrite(pins[id].ext, v);
#endif
}

uint16_t GPIO::analogRead(PinId id) {
#ifdef ARDUINO
	uint16_t last = ::analogRead(pins[id].ext);
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
	::analogWrite(pins[id].ext, v);
#endif
}

void GPIO::init() {
	for (uint8_t i = 0; i < MaxPins; i++) {
		PinDef *pd = pins + i;

		if (pd->ext > 0) {
#ifdef STM32
			STGPIO::getPortName(pd->ext, pd->info);

			GPIO_InitTypeDef gpio = { 0 };
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

			STGPIO::init(pd->ext, &gpio);
#elif ARDUINO
			if (pd->mode & PinModeOutput)
				pinMode(pd->ext, OUTPUT);
			else if (pd->mode & PinModeInput)
				pinMode(pd->ext, INPUT_PULLUP);
#endif
		}
	}
}
