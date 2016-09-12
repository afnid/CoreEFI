#ifndef _Hardware_h_
#define _Hardware_h_

#include "utils.h"

class Device;

class Device {
public:

	static uint16_t getBattery();
	static uint16_t getFreeMemory();

	static void flush();

	static uint16_t flashSize();
	static uint16_t flashSave(void *buf, uint16_t offset, uint16_t length);
	static uint16_t flashLoad(void *buf, uint16_t offset, uint16_t length);

	static void dump();
	static void init();
};

#endif
