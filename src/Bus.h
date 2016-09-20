#ifndef _Bus_h_
#define _Bus_h_

#include "Params.h"
#include "Pins.h"

class CanBus {
	bool status;

public:

	void init();

	uint16_t mem(bool alloced);

	bool send(uint32_t id, uint8_t *buf, uint8_t len);
	bool recv(uint32_t *id, uint8_t *buf, uint8_t *len);

	void sendParam(ParamTypeId id, uint16_t raw);
	void sendParam(PinId id, uint16_t v);
};

EXTERN CanBus canbus;

#endif
