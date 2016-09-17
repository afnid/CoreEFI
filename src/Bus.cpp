#include "System.h"
#include "Buffer.h"
#include "Tasks.h"
#include "Bus.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <SPI.h>
#include "mcp_can.h"

const int SPI_CS_PIN = 9;

MCP_CAN CAN(SPI_CS_PIN);

// MAX_CHAR_IN_MESSAGE = 8

#endif

static uint32_t checkBus(uint32_t now, void *data) {
	CanBus *bus = (CanBus *)data;
	uint8_t buf[8];
	uint32_t id;
	uint8_t len;

	if (bus->recv(&id, buf, &len)) {
#ifdef ARDUINO
		Serial.print(id, HEX);

		for (int i = 0; i < len; i++)
			Serial.print(buf[i], HEX);

		Serial.println();
#endif
	}

	return 0;
}

void CanBus::init() {
	status = false;
#ifdef ARDUINO
	status = CAN_OK == CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ);
#endif

	taskmgr.addTask(F("CanBus"), checkBus, this, 50);
}

uint16_t CanBus::mem(bool alloced) {
	return 0;
}

bool CanBus::send(uint32_t id, uint8_t *buf, uint8_t len) {
#ifdef ARDUINO
	return status && CAN.sendMsgBuf(id, 0, len, buf) == CAN_OK;
#else
	return false;
#endif
}

bool CanBus::recv(uint32_t *id, uint8_t *buf, uint8_t *len) {
#ifdef ARDUINO
	return status && CAN.readMsgBuf(id, len, buf) == CAN_OK;
#else
	return false;
#endif
}