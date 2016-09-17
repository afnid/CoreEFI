#include "Codes.h"
#include "Broker.h"

static void codescb(Buffer &send, BrokerEvent &be, void *data) {
	((Codes *)data)->send(send);
}

uint16_t Codes::init() {
	bzero(codes, sizeof(codes));

	broker.add(codescb, this, F("codes"));

	return sizeof(Codes);
}

