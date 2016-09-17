#include "Codes.h"
#include "Broker.h"

static void codescb(Buffer &send, BrokerEvent &be, void *data) {
	((Codes *)data)->send(send);
}

void Codes::init() {
	bzero(codes, sizeof(codes));

	broker.add(codescb, this, F("codes"));
}

uint16_t Codes::mem(bool alloced) {
	return 0;
}
