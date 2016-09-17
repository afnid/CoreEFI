#include "Vehicle.h"
#include "Hardware.h"
#include "Display.h"
#include "Params.h"
#include "GPIO.h"
#include "Tasks.h"

#ifdef ARDUINO
#include "FastLED.h"
#endif

#ifndef CONFIG
#define CONFIG 10
#endif

#define DATA_PIN 12
#define CLOCK_PIN 13

enum {
	PanelOil, PanelGen, PanelHiBeam, PanelLeft, PanelRight, PanelBacklight1, PanelBacklight2, PanelBacklight3, PanelBacklight4, NUM_LEDS,
};

#ifdef ARDUINO
CRGB leds[NUM_LEDS];
#endif

static const PinId config[1] = {
#if CONFIG == 1

		VehicleRadiatorTemp
		VehicleIsTurningLeft
		VehicleIsTurningRight
		VehicleIsLoBeamOn
		VehicleIsHiBeamOn
		VehicleIsParkingOn
		VehicleIsHornOn
		VehicleIsAirOn
		CalcFan1
		CalcFan2

#elif CONFIG == 2

		CalcFan1
		CalcFan2

		CalcEPAS1
		CalcEPAS2

		VehicleGauge1
		VehicleGauge2

		VehicleIsTurningLeft, 27
		VehicleIsTurningRight, 25
		VehicleIsParkingOn, 23
		VehicleIsTransReverse, 29

		VehicleIsLoBeamOn, 31
		VehicleIsHiBeamOn

		VehicleIsBrakeLeft, 33
		VehicleIsBrakeRight, 35

		SensorIsKeyAcc
		SensorIsKeyOn
		SensorIsCranking

		VehicleIsAirOn
		VehicleIsHornOn

		VehicleIsGenAlert
		VehicleIsOilAlert

		SensorIsKeyAcc, 44
		SensorIsKeyOn, 46
		SensorIsCranking, 48

		VehicleIsParkingOn, 32
		VehicleIsLoBeamOn, 40
		VehicleIsHiBeamOn
		VehicleIsClusterBright

		VehicleIsBrakeOn, 41
		VehicleIsAirOn, 43
		VehicleIsTransNeutral, 37
		VehicleIsTransReverse, 39

		VehicleIsHornOn, 45
		VehicleIsTurningLeft, 47
		VehicleIsTurningRight, 49

		VehicleIsFanSwitch1
		VehicleIsFanSwitch2
		VehicleIsHazardsOn
		VehicleIsInteriorOn

		VehicleIsMenuButton1, 8
		VehicleIsMenuButton2, 9

		VehicleRadiatorTemp
		SensorAMPS1
		SensorHEGO1
		SensorHEGO2
		SensorBAR
		SensorEGR
		SensorVCC
		SensorDEC
		SensorTPS
		SensorVSS
		SensorECT
		SensorMAF
		SensorACT
		VehicleFuelSender

#elif CONFIG == 3

		VehicleFuelSender
		VehicleIsTurningLeft
		VehicleIsTurningRight
		VehicleIsParkingOn
		VehicleIsTransReverse
		SensorIsKeyOn
		CalcFuelPump

#endif
	};

void Vehicle::sendStatus(Buffer &send, BrokerEvent &be) const {
	if (be.isMatch(send, F("fans"))) {
		send.p1(F("vehicle"));
		send.json(F("fan1duty"), fan1.duty);
		send.json(F("fan2duty"), fan2.duty);
		send.p2();
	} else
		send.nl("fans");
}

#include <stdio.h>

static void setPins() {
	uint8_t bits[bitsize(MaxPins)];
	bzero(bits, sizeof(bits));

	bitset(bits, 20);
	bitset(bits, 21);
	bitset(bits, 50);
	bitset(bits, 51);
	bitset(bits, 52);
	bitset(bits, 53);
	bitset(bits, DATA_PIN);
	bitset(bits, CLOCK_PIN);

	printf("bits %d %d\n", sizeof(bits), MaxPins);

	for (uint8_t i = 0; i < MaxPins; i++) {
		PinId id = (PinId) i;
		const GPIO::PinDef *pd = gpio.getPinDef(id);

		if (pd->ext)
			bitset(bits, pd->ext);
	}

	int input = 24;
	int output = 2;
	int analog = 5;

	for (uint8_t i = 0; i < MaxPins; i++) {
		PinId id = (PinId) i;
		GPIO::PinDef *pd = (GPIO::PinDef *) gpio.getPinDef(id);

		if (!pd->ext) {
			if (pd->mode & PinModeAnalog) {
				pd->ext = analog++ % 16;
			} else if (pd->mode & PinModeInput) {
				while (isset(bits, input))
					input++;

				pd->ext = min(53, input);
			} else if (pd->mode & PinModeOutput) {
				while (isset(bits, output))
					output++;

				pd->ext = min(53, output);
			}

			bitset(bits, pd->ext);

			if (input >= 49)
				input = 8;
		}
	}
}

static float scaleLinear(uint16_t v, uint16_t v1, uint16_t v2, float o1, float o2, bool clamp) {
	if (clamp && v <= v1)
		return o1;
	if (clamp && v >= v2)
		return o2;

	float pct = (v - v1) / (float) (v2 - v1);
	return o1 + (o2 - o1) * pct;
}

void Coded::init() {
	id = MaxPins;
	state = 0;
	value = 0;
	state = 0;
}

void Coded::service(uint32_t now) {
	const GPIO::PinDef *pd = gpio.getPinDef(id);

	if (pd) {
		int n = pd->getLast();

		if (state != n) {
			if (n)
				edge_hi = now;
			else {
				edge_lo = now;

				if (tdiff32(now, edge_hi > 10))
					count++;
			}
		}

		if (!state && count) {
			long ms = tdiff32(now, edge_lo);

			if (ms > 2000) {
				value *= 10;
				value += count;
				count = 0;
			}
		}
	}
}

Pulsed::Pulsed() {
	duty = 0;
	calced = 0;
}

int Pulsed::ramp(uint32_t now, uint16_t duty, uint16_t delay) {
	if (duty <= this->duty)
		return set(now, duty);

	if (tdiff32(now, calced) >= delay * 1000L) {
		int add = duty - this->duty;

		add = max(1, add * 10 / 100);

		if (this->duty > 20 && duty - this->duty > 30)
			add = 12;
		else if (this->duty > 20 && duty - this->duty > 16)
			add = 8;
		else if (duty - this->duty > 8)
			add = 3;

		add = 1;

		return set(now, duty + add);
	}

	return 0;
}

int Pulsed::set(uint32_t now, uint16_t duty) {
	if (this->duty != duty) {
		int change = duty - this->duty;
		calced = now;
		this->duty = duty;
		return change;
	}

	return 0;
}

#if 0
int rpmcount = 0;
static uint32_t lastrpm = 0;

static void rpmisr()
{
	rpmcount++;
}

static int getRpm2(uint32_t now)
{
	int rpm = -1;

	if (rpmcount >= 20) {
		if (lastrpm != 0) {
			// v8, 4 pulses/rev, r/m * 1r/4p * 60000ms/m = 60000/4
			uint32_t ms = tdiff32(now, lastrpm);
			rpm = (int)(15000L * rpmcount / ms);
		}

		rpmcount = 0;
		lastrpm = now;
	}

	return rpm;
}
#endif

inline uint16_t clamp(uint16_t v, uint16_t min, uint16_t max) {
	if (v < min)
		v = min;
	if (v > max)
		v = max;
	return v;
}

uint32_t Vehicle::getOnSeconds() const {
	const GPIO::PinDef *pd = gpio.getPinDef(IsKeyOn);
	return gpio.isPinSet(IsKeyOn) ? pd->ms() / 1000 : 0;
}

uint32_t Vehicle::getOffSeconds() const {
	const GPIO::PinDef *pd = gpio.getPinDef(IsKeyOn);
	return !gpio.isPinSet(IsKeyOn) ? pd->ms() / 1000 : 0;
}

bool Vehicle::isRunning() {
	return getParamUnsigned(SensorRPM) > 200;
}

void Vehicle::calcSteeringAssist(uint32_t now) {
	uint16_t v = isRunning() ? 20 : 0;
	setParamUnsigned(CalcEPAS1, v);

	uint16_t duty = 0;

	if (epas2.duty && getParamUnsigned(TimeRunSeconds) > 5) {
		uint16_t mph = getParamFloat(SensorVSS);

		if (mph <= 10)
			duty = clamp(duty, 0, 100 - mph * 10);
	}

	epas2.ramp(now, duty, 100);

	setParamUnsigned(CalcEPAS2, epas2.duty);
}

void Vehicle::calcFanSpeed(uint32_t now) {
	float mph = getParamFloat(SensorVSS);
	float temp = getParamFloat(VehicleRadiatorTemp);
	uint16_t duty = 0;

	if (mph < 50 && temp >= 165) {
		uint16_t max = gpio.isPinSet(IsClimateOn) ? 185 : 195;
		duty = scaleLinear(temp, 165, max, 0, 100, true);

		if (!isRunning()) {
			if (getParamUnsigned(TimeRunSeconds) < 20)
				duty = min(duty, 50);
			else
				duty = 0;
		}
	}

	if (fan1.ramp(now, duty, 4000) <= 5)
		fan2.ramp(now, duty, 4000);

	setParamUnsigned(CalcFan1, fan1.duty);
	setParamUnsigned(CalcFan2, fan2.duty);
}

void updateClusterWhites(int v) {
#ifdef ARDUINO
	leds[PanelBacklight1] = CRGB(v, v, v);
	leds[PanelBacklight2] = CRGB(v, v, v);
	leds[PanelBacklight3] = CRGB(v, v, v);
	leds[PanelBacklight4] = CRGB(v, v, v);
	FastLED.show();
#endif
}

void updateCluster(PinId id, int v) {
#ifdef ARDUINO
	switch (id) {
		case IsSignalLeft:
		leds[PanelLeft] = v ? CRGB::Green : CRGB::Black;
		break;
		case IsSignalRight:
		leds[PanelRight] = v ? CRGB::Green : CRGB::Black;
		break;
		case IsHiBeamOn:
		leds[PanelHiBeam] = v ? CRGB::Blue : CRGB::Black;
		break;
		case IsBrakeOn: //IsGenAlert:
		leds[PanelGen] = v ? CRGB::Red : CRGB::Black;
		break;
		case IsParkingOn://IsOilAlert:
		leds[PanelOil] = v ? CRGB::Red : CRGB::Black;
		break;
	}

	FastLED.show();
#endif
}

static void setBrakeLights() {
	uint16_t brakes = gpio.isPinSet(IsBrakeOn);
	uint16_t lt = gpio.isPinSet(IsSignalLeft);
	uint16_t rt = gpio.isPinSet(IsSignalRight);

	gpio.setPin(RelayBrakeLightLeft, brakes || lt);
	gpio.setPin(RelayBrakeLightRight, brakes || rt);
}

static void setParking() {
	uint16_t hi = gpio.isPinSet(IsHiBeamOn);
	uint16_t lo = gpio.isPinSet(IsLoBeamOn);

	if (hi || lo)
		gpio.setPin(IsParkingOn, true);
}

static void setTempGauge() {
	if (gpio.isPinSet(IsKeyOn)) {
		float temp = getParamFloat(VehicleRadiatorTemp);
		setParamFloat(VehicleGauge2, scaleLinear(temp, 150, 200, 0, 100, true));
	} else
		setParamFloat(VehicleGauge2, 0);
}

static void setFuelGauge() {
	setParamFloat(VehicleGauge1, getParamFloat(VehicleGauge2));
}

void Vehicle::serviceInput(uint32_t now) {
	if (++inpin >= MaxPins)
		inpin = 0;

	const GPIO::PinDef *pd = gpio.getPinDef((PinId) inpin);

	if (pd && (pd->mode & PinModeInput)) {
		const uint16_t TESTLO = 400;
		const uint16_t TESTHI = 700;

		switch (pd->getId()) {
		case AnalogRadiatorTemp:
			setParamFloat(VehicleRadiatorTemp, scaleLinear(pd->getLast(), TESTLO, TESTHI, 150, 210, false));
			setTempGauge();
			break;
		case AnalogAMPS1:
			setParamFloat(SensorAMPS1, scaleLinear(pd->getLast(), 512, 610, 500, 4000, false));
			break;
		case IsMenuButton1:
			if (pd->getLast())
				display.menuInput(0);
			break;
		case IsMenuButton2:
			if (!pd->getLast())
				display.menuInput(1);
			break;
		case AnalogFuel:
			setFuelGauge();
			break;
		default:
			break;
		}
	}
}

void Vehicle::serviceOutput(uint32_t now) {
	if (++outpin >= MaxPins)
		outpin = 0;

	const GPIO::PinDef *pd = gpio.getPinDef((PinId) outpin);

	static const GPIO::PinDef *bl = 0;
	static const GPIO::PinDef *br = 0;

	if (pd && (pd->mode & PinModeOutput)) {
		switch (pd->getId()) {
		case RelayFan1:
		case RelayFan2:
			calcFanSpeed(now);
			break;
		case RelayEPAS1:
		case RelayEPAS2:
			calcSteeringAssist(now);
			break;
		case IsParkingOn:
			setParking();
			break;
		case RelayBrakeLightLeft:
			if (turning)
				return;
			setBrakeLights();
			bl = pd;
			break;
		case RelayBrakeLightRight:
			if (turning)
				return;
			setBrakeLights();
			br = pd;
			break;
		default:
			break;
		}

		uint16_t v = gpio.isPinSet(pd->getId());

		switch (pd->getId()) {
		case RelayGauge1:
			v = scaleLinear(v, 0, 100, 0, 100, true);
			break;
		case RelayFan1:
		case RelayFan2:
			v = v <= 0 ? 0 : scaleLinear(v, 0, 100, 80, 255, true);
			break;
		case CalcEPAS2:
			v = scaleLinear(v, 0, 100, 0, 100, true);
			break;
		case IsSignalLeft:
		case IsSignalRight:
			turning = v != 0;

			setBrakeLights();

			if (v) {
				if (pd->getLast() && pd->ms() >= 600)
					v = 0;
				else if (!pd->getLast() && pd->ms() >= 1200)
					v = 1;
				else
					v = pd->getLast();

				if (bl && pd->getId() == IsSignalLeft)
					gpio.setPin(bl->getId(), v);
				//if (br && pd->getId() == VehicleIsTurningRight)
				//br->writePin(v);
			}

			updateCluster(pd->getId(), v);
			break;
		case RelayBrakeLightLeft:
		case RelayBrakeLightRight:
			if (gpio.isPinSet(IsBrakeOn)) {
				if (brakeFlash < 6) {
					if (pd->ms() >= 50) {
						v = !pd->getLast();
						brakeFlash++;
					} else
						v = pd->getLast();
				}

				if (bl)
					gpio.setPin(bl->getId(), v);
				if (br)
					gpio.setPin(br->getId(), v);
			} else
				brakeFlash = 0;
			break;
		default:
			break;
		}

		gpio.setPin(pd->getId(), v);
	}
}

void Vehicle::checkVehicle(uint32_t now) {
	for (int i = 0; i < 10; i++)
		serviceInput(now);

	if (isRunning()) {
		if (!gpio.isPinSet(IsParkingOn) && getParamUnsigned(TimeRunSeconds) > 3)
			gpio.setPin(IsParkingOn, true);
		if (!gpio.isPinSet(IsLoBeamOn) && getParamUnsigned(TimeRunSeconds) > 5)
			gpio.setPin(IsLoBeamOn, true);
	} else {
		if (gpio.isPinSet(IsLoBeamOn) && getParamUnsigned(TimeRunSeconds) > 3)
			gpio.setPin(IsLoBeamOn, false);
		if (gpio.isPinSet(IsParkingOn) && getParamUnsigned(TimeRunSeconds) > 5)
			gpio.setPin(IsParkingOn, false);
	}

	for (int i = 0; i < 10; i++)
		serviceOutput(now);

	/*
	 odb1.service(now);
	 */
}

static uint32_t taskcb(uint32_t now, void *data) {
	((Vehicle *) data)->checkVehicle(now);
	return 0;
}

void Vehicle::brokercb(Buffer &send, BrokerEvent &be, void *data) {
	((Vehicle *) data)->sendStatus(send, be);
}

void Vehicle::init() {
	brakeFlash = 0;

#ifdef ARDUINO
	LEDS.addLeds<WS2812,2,RGB>(leds,NUM_LEDS);
	LEDS.setBrightness(84);
#endif

	setPins();

	taskmgr.addTask(F("Vehicle"), taskcb, this, 20);
	broker.add(brokercb, this, F("c"));
}

uint16_t Vehicle::mem(bool alloced) {
	return alloced ? 0 : sizeof(config);
}
