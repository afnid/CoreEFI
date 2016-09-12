#ifdef STM32

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "stm32_main.h"
#include "GPIO.h"
#include "System.h"
#include "Channel.h"
#include "Tasks.h"

extern "C" {

void ErrorHandler() {
}

}

bool VCP_avail() {
	return 0;
}

char VCP_getchar() {
	return 0;
}

int VCP_get_char(uint8_t *buf) {
	return 0;
}

void VCP_put_char(uint8_t ch) {
}

void VCP_send_buffer(uint8_t *buf, int len) {
}

// Private variables
volatile uint32_t time_var1;
volatile uint32_t time_var2;

//__ALIGN_BEGIN USB_OTG_CORE_HANDLE USB_OTG_dev __ALIGN_END;

static GPIO::PinId ports[] = {
	GPIO::Led1,
	GPIO::Led2,
	GPIO::Led3,
	GPIO::Led4,
};

int nports = 4;

/*
 * Called from systick handler
 */
void timing_handler() {
	if (time_var1)
		time_var1--;

	time_var2++;
}

static void sleep(volatile uint32_t ms) {
	time_var1 = ms * 1000;
	while (time_var1)
		;
}

static void setall() {
	for (uint8_t i = 0; i < nports; i++)
		GPIO::setPin(ports[i], 1);
}

static void clrall() {
	for (uint8_t i = 0; i < nports; i++)
		GPIO::setPin(ports[i], 0);
}

static void blip(GPIO::PinId id, uint16_t ms) {
	GPIO::setPin(id, 1);
	sleep(ms);
	GPIO::setPin(id, 0);
	sleep(ms);
}

static void circ(uint16_t ms) {
	static uint16_t port = 0;

	blip(ports[port], ms);

	if (++port >= 4)
		port = 0;
}

static void init() {
	// ---------- SysTick timer -------- //
	if (SysTick_Config(SystemCoreClock / 1000000L)) {
		// Capture error
		while (1) {
		};
	}

	// ---------- GPIO -------- //
	// GPIOD Periph clock enable
	//RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	// Configure PD12, PD13, PD14 and PD15 in output pushpull mode
	/*
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	// ------------- USB -------------- //
	//USBD_Init(&USB_OTG_dev, USB_OTG_FS_CORE_ID, &USR_desc, &USBD_CDC_cb, &USR_cb);

	*/
	NVIC_SetPriority(SysTick_IRQn, 0);
}

/*
 * Dummy function to avoid compiler error
 */
void _init() {
}

uint32_t millis() {
	return 0;
}

int main(void) {
	init();

	/*
	 * Disable STDOUT buffering. Otherwise nothing will be printed
	 * before a newline character or when the buffer is flushed.
	 */
	setbuf(stdout, NULL);

	uint16_t loop = 0;

	for (int ms = 100; ms <= 500; ms += 100) {
		for (int i = 0; i < 10; i++) {
			printf("Loop: %i\n", ++loop);
			circ(ms);
		}
	}

	while (1) {
		clrall();
		sleep(1000);
		setall();
		sleep(1000);

		printf("Loop: %i\n", ++loop);

		if (loop == 20) {
			clrall();

			initSystem(true);

			printf("Loop: %i\n", ++loop);
			sleep(3000);
			TaskMgr::runTasks();
		}
	}

	return 0;
}

#endif
