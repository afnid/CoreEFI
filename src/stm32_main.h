#ifndef _stm32_main_h_
#define _stm32_main_h_

#ifdef STM32

#include "utils.h"

#include "GPIO.h"
#include "System.h"

uint32_t HAL_GetTick();

void VCP_put_char(uint8_t buf);
void VCP_send_str(uint8_t* buf);
int VCP_get_char(uint8_t *buf);
int VCP_get_string(uint8_t *buf);
void VCP_send_buffer(uint8_t* buf, int len);
bool VCP_has_char();

extern "C" {
void ErrorHandler();
}

class STRCC {
public:

	void init();
};

class STWDG {
	WWDG_HandleTypeDef wwdg;

public:

	void init();
	void refresh();
};

class System {
public:

	volatile uint16_t tim2;
	volatile uint16_t tim3;

	uint8_t count;
	volatile uint8_t press;
	volatile uint8_t debug;

	STRCC rcc;
	STWDG wdg;

	void blip(GPIO::PinId pt, uint16_t ms);
	void blink(GPIO::PinId pt, uint8_t count);
	void blink(GPIO::PinId pt, uint8_t count, uint8_t repeats, uint16_t flash = 250, uint16_t sleep = 2000);

	void irq_enable(IRQn_Type irq, uint8_t pri, uint8_t sub);
	void irq_disable(IRQn_Type irq);
};

EXTERN System global;


#endif
#endif
