#ifndef _Display_h_
#define _Display_h_

#include "Broker.h"

class Display {
	char display[4][21];

	int menu;
	int row;

	void showLine(int line, char *buf);
	void sendBroker(Buffer &send, BrokerEvent &be);
	static void brokercb(Buffer &send, BrokerEvent &be, void *data);

public:

	void init();

	uint16_t mem(bool alloced);

	void showDisplay(uint32_t now);

	void menuInput(uint8_t id);
};

EXTERN Display display;

#endif
