#ifndef _Timers_h_
#define _Timers_h_

class Timers {
public:

	static void init();

	static uint16_t mem(bool alloced);

	static void sleep(uint32_t us);
};

#endif
