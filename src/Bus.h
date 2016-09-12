#ifndef _Bus_h_
#define _Bus_h_

class CanBus {
	bool status;

public:

	uint8_t init();
	bool send(uint32_t id, uint8_t *buf, uint8_t len);
	bool recv(uint32_t *id, uint8_t *buf, uint8_t *len);
};

#endif
