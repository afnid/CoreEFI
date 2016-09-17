#ifndef _Strategy_h_
#define _Strategy_h_

#include "Params.h"

class Strategy {
public:

	static void init();

	static uint16_t mem(bool alloced);

	static void runStrategy();
	static void setTimer(ParamTypeId id, bool enable);
};

#endif
