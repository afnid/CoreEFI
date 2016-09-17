#include "Vehicle.h"
#include "GPIO.h"
#include "Hardware.h"
#include "Display.h"
#include "Params.h"
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

static const GPIO::PinId config[1] = {
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

void Vehicle::prompt_cb(Buffer &send, BrokerEvent &be, void *data) {
	((Vehicle *) data)->sendStatus(send);
}

void Vehicle::sendStatus(Buffer &send) const {
	send.p1(F("vehicle"));
	send.json(F("fan1duty"), fan1.duty);
	send.json(F("fan2duty"), fan2.duty);
	send.p2();
}

#include <stdio.h>

static void setPins() {
	uint8_t bits[bitsize(GPIO::MaxPins)];
	bzero(bits, sizeof(bits));

	bitset(bits, 20);
	bitset(bits, 21);
	bitset(bits, 50);
	bitset(bits, 51);
	bitset(bits, 52);
	bitset(bits, 53);
	bitset(bits, DATA_PIN);
	bitset(bits, CLOCK_PIN);

	printf("bits %d %d\n", sizeof(bits), GPIO::MaxPins);

	for (uint8_t i = 0; i < GPIO::MaxPins; i++) {
		GPIO::PinId id = (GPIO::PinId) i;
		const GPIO::PinDef *pd = GPIO::getPinDef(id);

		if (pd->ext)
			bitset(bits, pd->ext);
	}

	int input = 24;
	int output = 2;
	int analog = 5;

	for (uint8_t i = 0; i < GPIO::MaxPins; i++) {
		GPIO::PinId id = (GPIO::PinId) i;
		GPIO::PinDef *pd = (GPIO::PinDef *) GPIO::getPinDef(id);

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
	id = GPIO::MaxPins;
	state = 0;
	value = 0;
	state = 0;
}

void Coded::service(uint32_t now) {
	const GPIO::PinDef *pi = GPIO::getPinDef(id);

	if (pi) {
		int n = pi->getLast();

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
	const GPIO::PinDef *pd = GPIO::getPinDef(GPIO::IsKeyOn);
	return GPIO::isPinSet(GPIO::IsKeyOn) ? pd->ms() / 1000 : 0;
}

uint32_t Vehicle::getOffSeconds() const {
	const GPIO::PinDef *pd = GPIO::getPinDef(GPIO::IsKeyOn);
	return !GPIO::isPinSet(GPIO::IsKeyOn) ? pd->ms() / 1000 : 0;
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
		uint16_t max = GPIO::isPinSet(GPIO::IsClimateOn) ? 185 : 195;
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

void updateCluster(GPIO::PinId id, int v) {
#ifdef ARDUINO
	switch (id) {
		case GPIO::IsSignalLeft:
		leds[PanelLeft] = v ? CRGB::Green : CRGB::Black;
		break;
		case GPIO::IsSignalRight:
		leds[PanelRight] = v ? CRGB::Green : CRGB::Black;
		break;
		case GPIO::IsHiBeamOn:
		leds[PanelHiBeam] = v ? CRGB::Blue : CRGB::Black;
		break;
		case GPIO::IsBrakeOn: //IsGenAlert:
		leds[PanelGen] = v ? CRGB::Red : CRGB::Black;
		break;
		case GPIO::IsParkingOn://IsOilAlert:
		leds[PanelOil] = v ? CRGB::Red : CRGB::Black;
		break;
	}

	FastLED.show();
#endif
}

static void setBrakeLights() {
	uint16_t brakes = GPIO::isPinSet(GPIO::IsBrakeOn);
	uint16_t lt = GPIO::isPinSet(GPIO::IsSignalLeft);
	uint16_t rt = GPIO::isPinSet(GPIO::IsSignalRight);

	GPIO::setPin(GPIO::RelayBrakeLightLeft, brakes || lt);
	GPIO::setPin(GPIO::RelayBrakeLightRight, brakes || rt);
}

static void setParking() {
	uint16_t hi = GPIO::isPinSet(GPIO::IsHiBeamOn);
	uint16_t lo = GPIO::isPinSet(GPIO::IsLoBeamOn);

	if (hi || lo)
		GPIO::setPin(GPIO::IsParkingOn, true);
}

static void setTempGauge() {
	if (GPIO::isPinSet(GPIO::IsKeyOn)) {
		float temp = getParamFloat(VehicleRadiatorTemp);
		setParamFloat(VehicleGauge2, scaleLinear(temp, 150, 200, 0, 100, true));
	} else
		setParamFloat(VehicleGauge2, 0);
}

static void setFuelGauge() {
	setParamFloat(VehicleGauge1, getParamFloat(VehicleGauge2));
}

void Vehicle::serviceInput(uint32_t now) {
	if (++inpin > GPIO::MaxPins)
		inpin = 0;

	const GPIO::PinDef *pi = GPIO::getPinDef((GPIO::PinId) inpin);

	if (pi && (pi->mode & PinModeInput)) {
		const uint16_t TESTLO = 400;
		const uint16_t TESTHI = 700;

		switch (pi->getId()) {
		case GPIO::AnalogRadiatorTemp:
			setParamFloat(VehicleRadiatorTemp, scaleLinear(pi->getLast(), TESTLO, TESTHI, 150, 210, false));
			setTempGauge();
			break;
		case GPIO::AnalogAMPS1:
			setParamFloat(SensorAMPS1, scaleLinear(pi->getLast(), 512, 610, 500, 4000, false));
			break;
		case GPIO::IsMenuButton1:
			if (!pi->getLast())
				display.menuInput(hardware.send(), 0);
			break;
		case GPIO::IsMenuButton2:
			if (!pi->getLast())
				display.menuInput(hardware.send(), 1);
			break;
		case GPIO::AnalogFuel:
			setFuelGauge();
			break;
		default:
			break;
		}
	}
}

void Vehicle::serviceOutput(uint32_t now) {
	if (++outpin > GPIO::MaxPins)
		outpin = 0;

	const GPIO::PinDef *pi = GPIO::getPinDef((GPIO::PinId) outpin);

	static const GPIO::PinDef *bl = 0;
	static const GPIO::PinDef *br = 0;

	if (pi && (pi->mode & PinModeOutput)) {
		switch (pi->getId()) {
		case GPIO::RelayFan1:
		case GPIO::RelayFan2:
			calcFanSpeed(now);
			break;
		case GPIO::RelayEPAS1:
		case GPIO::RelayEPAS2:
			calcSteeringAssist(now);
			break;
		case GPIO::IsParkingOn:
			setParking();
			break;
		case GPIO::RelayBrakeLightLeft:
			if (turning)
				return;
			setBrakeLights();
			bl = pi;
			break;
		case GPIO::RelayBrakeLightRight:
			if (turning)
				return;
			setBrakeLights();
			br = pi;
			break;
		default:
			break;
		}

		uint16_t v = GPIO::isPinSet(pi->getId());

		switch (pi->getId()) {
		case GPIO::RelayGauge1:
			v = scaleLinear(v, 0, 100, 0, 100, true);
			break;
		case GPIO::RelayFan1:
		case GPIO::RelayFan2:
			v = v <= 0 ? 0 : scaleLinear(v, 0, 100, 80, 255, true);
			break;
		case CalcEPAS2:
			v = scaleLinear(v, 0, 100, 0, 100, true);
			break;
		case GPIO::IsSignalLeft:
		case GPIO::IsSignalRight:
			turning = v != 0;

			setBrakeLights();

			if (v) {
				if (pi->getLast() && pi->ms() >= 600)
					v = 0;
				else if (!pi->getLast() && pi->ms() >= 1200)
					v = 1;
				else
					v = pi->getLast();

				if (bl && pi->getId() == GPIO::IsSignalLeft)
					GPIO::setPin(bl->getId(), v);
				//if (br && pi->getId() == VehicleIsTurningRight)
				//br->writePin(v);
			}

			updateCluster(pi->getId(), v);
			break;
		case GPIO::RelayBrakeLightLeft:
		case GPIO::RelayBrakeLightRight:
			if (GPIO::isPinSet(GPIO::IsBrakeOn)) {
				if (brakeFlash < 6) {
					if (pi->ms() >= 50) {
						v = !pi->getLast();
						brakeFlash++;
					} else
						v = pi->getLast();
				}

				if (bl)
					GPIO::setPin(bl->getId(), v);
				if (br)
					GPIO::setPin(br->getId(), v);
			} else
				brakeFlash = 0;
			break;
		default:
			break;
		}

		GPIO::setPin(pi->getId(), v);
	}
}

void Vehicle::checkVehicle(uint32_t now) {
	for (int i = 0; i < 10; i++)
		serviceInput(now);

	if (isRunning()) {
		if (!GPIO::isPinSet(GPIO::IsParkingOn) && getParamUnsigned(TimeRunSeconds) > 3)
			GPIO::setPin(GPIO::IsParkingOn, true);
		if (!GPIO::isPinSet(GPIO::IsLoBeamOn) && getParamUnsigned(TimeRunSeconds) > 5)
			GPIO::setPin(GPIO::IsLoBeamOn, true);
	} else {
		if (GPIO::isPinSet(GPIO::IsLoBeamOn) && getParamUnsigned(TimeRunSeconds) > 3)
			GPIO::setPin(GPIO::IsLoBeamOn, false);
		if (GPIO::isPinSet(GPIO::IsParkingOn) && getParamUnsigned(TimeRunSeconds) > 5)
			GPIO::setPin(GPIO::IsParkingOn, false);
	}

	for (int i = 0; i < 10; i++)
		serviceOutput(now);

	/*
	 odb1.service(now);
	 */
}

static uint32_t runVehicle(uint32_t now, void *data) {
	((Vehicle *) data)->checkVehicle(now);
	return 0;
}

void Vehicle::init() {
	brakeFlash = 0;

#ifdef ARDUINO
	LEDS.addLeds<WS2812,2,RGB>(leds,NUM_LEDS);
	LEDS.setBrightness(84);
#endif

	setPins();

	taskmgr.addTask(F("Vehicle"), runVehicle, this, 20);
	broker.add(prompt_cb, this, F("vehicle"));
}

uint16_t Vehicle::mem(bool alloced) {
	return alloced ? 0 : sizeof(config);
}
