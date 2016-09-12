#ifndef _stm32_main_h_
#define _stm32_main_h_

#include <stdint.h>

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

#include "System.h"

void VCP_put_char(uint8_t buf);
void VCP_send_str(uint8_t* buf);
int VCP_get_char(uint8_t *buf);
int VCP_get_string(uint8_t *buf);
void VCP_send_buffer(uint8_t* buf, int len);
bool VCP_has_char();

extern "C" {
void ErrorHandler();
}

#endif
