// copyright to me, released under GPL V3

#define EXTERN

#include "Tasks.h"
#include "Hardware.h"
#include "GPIO.h"

#include "Schedule.h"
#include "Strategy.h"
#include "Codes.h"
#include "Encoder.h"
#include "Timers.h"

#include "Epoch.h"
#include "Stream.h"

#include "Display.h"
#include "Vehicle.h"
#include "Bus.h"

#ifndef NDEBUG
#define MYNAME(x)	x, F(#x)
#else
#define MYNAME(x)	x, 0
#endif

#include "efi_pins.h"

static uint16_t add(uint16_t &total, uint16_t mem) {
	total += mem;
	return mem;
}

static void setDefaults() {
	setParamUnsigned(ConstTicksInUS, TICKTOUS);

	setParamUnsigned(SensorDEC, 22111);
	setParamUnsigned(SensorDEC, 1000);
	setParamUnsigned(SensorDEC, 3000);
	setParamUnsigned(SensorDEC, getParamUnsigned(ConstIdleRPM) / 2);

	setParamFloat(SensorHEGO1, 1);
	setParamFloat(SensorHEGO2, 1);
	setParamFloat(SensorECT, 190);
	setParamFloat(SensorACT, 150);
	setParamFloat(SensorMAF, 5);
	setParamFloat(SensorTPS, 20);
	setParamFloat(SensorVCC, 14);
	setParamUnsigned(SensorGEAR, 5);
	gpio.setPin(IsKeyOn, 1);
}

static void mem(Buffer &send, bool alloced) {
	uint16_t total = 0;
	send.json(F("type"), alloced ? "alloc" : "status");
	send.json(F("tasks"), add(total, taskmgr.mem(alloced)));
	send.json(F("broker"), add(total, broker.mem(alloced)));

	send.json(F("epoch"), add(total, Epoch::mem(alloced)));
	send.json(F("params"), add(total, Params::mem(alloced)));
	send.json(F("codes"), add(total, codes.mem(alloced)));
	send.json(F("vehicle"), add(total, vehicle.mem(alloced)));
	send.json(F("display"), add(total, display.mem(alloced)));
	send.json(F("canbus"), add(total, canbus.mem(alloced)));

	send.json(F("encoder"), add(total, encoder.mem(alloced)));
	send.json(F("decoder"), add(total, decoder.mem(alloced)));
	send.json(F("schedule"), add(total, BitSchedule::mem(alloced)));
	send.json(F("plan"), add(total, BitPlan::mem(alloced)));
	send.json(F("strategy"), add(total, Strategy::mem(alloced)));
	send.json(F("timers"), add(total, Timers::mem(alloced)));

	send.json(F("bytes"), total);
	send.p2();
}

static void brokercb(Buffer &send, BrokerEvent &be, void *data) {
	if (be.isMatch(send, F("defaults"))) {
		setDefaults();
	} else if (be.isMatch(send, F("mem"))) {
		mem(send, be.nextInt(0) == 0);
	} else if (be.isMatch(send, F("sizes"))) {
		uint16_t total = 0;

		send.json(F("clock"), add(total, sizeof(Epoch)));
		send.json(F("prompt"), add(total, sizeof(Broker)));
		send.json(F("codes"), add(total, sizeof(Codes)));

		send.json(F("display"), add(total, sizeof(Display)));
		send.json(F("vehicle"), add(total, sizeof(Vehicle)));

		send.json(F("strategy"), add(total, sizeof(Strategy)));
		send.json(F("encoder"), add(total, sizeof(Encoder)));
		send.json(F("decoder"), add(total, sizeof(Decoder)));
		send.json(F("schedule"), add(total, sizeof(BitSchedule)));
		send.json(F("events"), add(total, sizeof(BitPlan)));
		send.json(F("timers"), add(total, sizeof(Timers)));

		send.json(F("bytes"), total);
		send.p2();
	} else if (be.isMatch(send, F("config"))) {
		send.json(F("maxcyls"), MaxCylinders);
		send.json(F("maxcoils"), MaxCoils);
		send.json(F("maxteeth"), MaxEncoderTeeth);
		send.json(F("cyls"), getParamUnsigned(ConstCylinders));
		send.json(F("coils"), getParamUnsigned(ConstCoils));
		send.json(F("teeth"), getParamUnsigned(ConstEncoderTeeth));
		send.json(F("pulse"), (uint16_t) sizeof(pulse_t));
		send.p2();
	} else
		send.nl("defaults|mem|config");
}

static uint32_t brokertask(uint32_t now, void *data) {
	((Broker *) data)->run(hardware.send(), hardware.recv());
	return 0;
}

void initSystem(bool doefi) {
	hardware.init();
	gpio.init(pins, MaxPins);

	Epoch::init();
	Params::init();

	codes.init();
	vehicle.init();
	display.init();
	canbus.init();

	if (doefi) {
		encoder.init();
		decoder.init();
		BitSchedule::init();
		BitPlan::init();
		Strategy::init();
		Timers::init();
	}

	broker.add(brokercb, 0, F("sys"));
	taskmgr.addTask(F("Broker"), brokertask, &broker, 400);

	setDefaults();

	if (doefi) {
		encoder.refresh();
		decoder.refresh(clockTicks());
	}
}

