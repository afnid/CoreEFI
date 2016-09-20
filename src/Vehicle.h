#ifndef _Vehicle_h_
#define _Vehicle_h_

#include "Broker.h"
#include "Pins.h"

class Coded {
public:
	PinId id;
	uint16_t value;
	uint16_t state;
	uint16_t count;
	uint32_t edge_lo;
	uint32_t edge_hi;

	void init();
	void service(uint32_t now);
	void json(const flash_t *name, Buffer &send) const;
};

class Pulsed {
	int set(uint32_t now, uint16_t duty);

public:
	uint16_t duty;
	uint32_t calced;

	Pulsed();
	int ramp(uint32_t now, uint16_t duty, uint16_t delay);
	void json(const flash_t *name, Buffer &send) const;
};

class Vehicle {
	uint8_t inpin;
	uint8_t outpin;

	uint8_t brakeFlash;
	bool turning;

	Coded odb1;

	Pulsed fan1;
	Pulsed fan2;

	Pulsed epas;

	void json(Buffer &send) const;

	static void brokercb(Buffer &send, BrokerEvent &be, void *data);
	static uint32_t taskcb(uint32_t now, void *data);

	void checkVehicle(uint32_t now);
	bool isRunning();
	void fans_pwm(uint32_t now);
	int getSteeringAssistDuty(uint32_t now);
	int getFanDuty(uint32_t now);
	void serviceInput(uint32_t now);
	void serviceOutput(uint32_t now);

	void calcSteeringAssist(uint32_t now);
	void calcFanSpeed(uint32_t now);
	void sendStatus(Buffer &send, BrokerEvent &be) const;

	uint32_t getOnSeconds() const;
	uint32_t getOffSeconds() const;

public:

	void init();

	uint16_t mem(bool alloced);
};

EXTERN Vehicle vehicle;

#endif
