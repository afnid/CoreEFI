// copyright to me, released under GPL V3

#ifndef _Pins_h_
#define _Pins_h_

#include "GPIO.h"

class PinMgr {
public:

	//static uint16_t initPins();
	//static void readPins();

	static GPIO::PinId getInjectorPin(uint8_t cyl);
	static GPIO::PinId getSparkPin(uint8_t cyl);
	//static void resetOutputPins();

	//static const GPIO::PinDef *readNextInputPin();
	//static const GPIO::PinDef *getNextOutputPin();
	//static const GPIO::PinDef *getPinById(GPIO::PinId id);
	//static const GPIO::PinDef *getPinByIdx1(GPIO::PinId id);
	//static const GPIO::PinDef *getLast(GPIO::PinId idx);
};

#endif
