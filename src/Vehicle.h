#ifndef _Vehicle_h_
#define _Vehicle_h_

#include "Pins.h"

class Coded {
public:
	GPIO::PinId id;
	uint16_t value;
	uint16_t state;
	uint16_t count;
	uint32_t edge_lo;
	uint32_t edge_hi;

	void init();
	void service(uint32_t now);
};

class Pulsed {
	int set(uint32_t now, uint16_t duty);

public:
	uint16_t duty;
	uint32_t calced;

	Pulsed();
	int ramp(uint32_t now, uint16_t duty, uint16_t delay);
};

class Vehicle {
	uint8_t inpin;
	uint8_t outpin;

	uint8_t brakeFlash;
	bool turning;

	Coded odb1;

	Pulsed fan1;
	Pulsed fan2;

	Pulsed epas2;

	static void prompt_cb(Buffer &send, void *data);

	bool isRunning();
	void fans_pwm(uint32_t now);
	int getSteeringAssistDuty(uint32_t now);
	int getFanDuty(uint32_t now);
	void serviceInput(uint32_t now);
	void serviceOutput(uint32_t now);

	void calcSteeringAssist(uint32_t now);
	void calcFanSpeed(uint32_t now);
	void sendStatus(Buffer &send) const;

	uint32_t getOnSeconds() const;
	uint32_t getOffSeconds() const;

public:

	uint16_t init();
	void checkVehicle(uint32_t now);
};

EXTERN Vehicle vehicle;

#endif
