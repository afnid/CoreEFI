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

volatile uint32_t time_var1;
volatile uint32_t time_var2;

int nports = 4;

/*
 * Called from systick handler
 */
void timing_handler() {
	if (time_var1)
		time_var1--;

	time_var2++;
}

uint32_t millis() {
	return 0;
}

int main(void) {
	HAL_Init();

	global.rcc.init();
	global.wdg.init();

	setbuf(stdout, NULL);

	initSystem(false);

	while (true) {
		global.wdg.refresh();
	}
}


#endif
