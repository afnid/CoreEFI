#ifdef ARDUINO
#include "Arduino.h"
#include "FastLED.h"
#endif

#include "System.h"
#include "Buffer.h"
#include "Shell.h"
#include "Params.h"
#include "Vehicle.h"
#include "Tasks.h"
#include "Display.h"
#include "Codes.h"
#include "GPIO.h"

#ifndef CONFIG
#define CONFIG 10
#endif

#define DATA_PIN 12
#define CLOCK_PIN 13

enum {
	PanelOil,
	PanelGen,
	PanelHiBeam,
	PanelLeft,
	PanelRight,
	PanelBacklight1,
	PanelBacklight2,
	PanelBacklight3,
	PanelBacklight4,
	NUM_LEDS,
};

#ifdef ARDUINO
CRGB leds[NUM_LEDS];
#endif

#if CONFIG == 1

static GPIO::Def pins[] = {
    { GPIO::ModeInputAnalog, VehicleRadiatorTemp },
    { GPIO::ModeOutput, VehicleIsTurningLeft },
    { GPIO::ModeOutput, VehicleIsTurningRight },
    { GPIO::ModeOutput, VehicleIsLoBeamOn },
    { GPIO::ModeOutput, VehicleIsHiBeamOn },
    { GPIO::ModeOutput, VehicleIsParkingOn },
    { GPIO::ModeOutput, VehicleIsHornOn },
    { GPIO::ModeOutput, VehicleIsAirOn },
    { GPIO::ModeOutput, CalcFan1 },
    { GPIO::ModeOutput, CalcFan2 },
	{ 0 },
};

#elif CONFIG == 2

static GPIO::Def pins[] = {
    { 0, GPIO::ModeOutputPWM, CalcFan1 },
    { 0, GPIO::ModeOutputPWM, CalcFan2 },

	{ 0, GPIO::ModeOutputPWM, CalcEPAS1 },
	{ 0, GPIO::ModeOutputPWM, CalcEPAS2 },

	{ 0, GPIO::ModeOutputPWM, VehicleGauge1 },
	{ 0, GPIO::ModeOutputPWM, VehicleGauge2 },

    { 1, GPIO::ModeOutput, VehicleIsTurningLeft, 27 },
    { 1, GPIO::ModeOutput, VehicleIsTurningRight, 25 },
    { 1, GPIO::ModeOutput, VehicleIsParkingOn, 23 },
    { 1, GPIO::ModeOutput, VehicleIsTransReverse, 29 },

    { 1, GPIO::ModeOutput, VehicleIsLoBeamOn, 31 },
    { 1, GPIO::ModeOutput, VehicleIsHiBeamOn },

    { 1, GPIO::ModeOutput, VehicleIsBrakeLeft, 33 },
    { 1, GPIO::ModeOutput, VehicleIsBrakeRight, 35 },

	{ 1, GPIO::ModeOutput, SensorIsKeyAcc },
	{ 1, GPIO::ModeOutput, SensorIsKeyOn },
	{ 1, GPIO::ModeOutput, SensorIsCranking },

    { 1, GPIO::ModeOutput, VehicleIsAirOn },
    { 1, GPIO::ModeOutput, VehicleIsHornOn },

    { 1, GPIO::ModeOutput, VehicleIsGenAlert },
    { 1, GPIO::ModeOutput, VehicleIsOilAlert },

    { 1, GPIO::ModeInputPullup, SensorIsKeyAcc, 44 },
    { 1, GPIO::ModeInputPullup, SensorIsKeyOn, 46 },
    { 1, GPIO::ModeInputPullup, SensorIsCranking, 48 },

    { 1, GPIO::ModeInputPullup, VehicleIsParkingOn, 32 },
    { 1, GPIO::ModeInputPullup, VehicleIsLoBeamOn, 40 },
    { 1, GPIO::ModeInputPullup, VehicleIsHiBeamOn },
    { 1, GPIO::ModeInputPullup, VehicleIsClusterBright },

    { 1, GPIO::ModeInputPullup, VehicleIsBrakeOn, 41 },
	{ 1, GPIO::ModeInputPullup, VehicleIsAirOn, 43 },
    { 1, GPIO::ModeInputPullup, VehicleIsTransNeutral, 37 },
    { 1, GPIO::ModeInputPullup, VehicleIsTransReverse, 39 },

    { 1, GPIO::ModeInputPullup, VehicleIsHornOn, 45 },
    { 0, GPIO::ModeInputPullup, VehicleIsTurningLeft, 47 },
    { 0, GPIO::ModeInputPullup, VehicleIsTurningRight, 49 },

    { 1, GPIO::ModeInputPullup, VehicleIsFanSwitch1 },
    { 1, GPIO::ModeInputPullup, VehicleIsFanSwitch2 },
    { 1, GPIO::ModeInputPullup, VehicleIsHazardsOn },
    { 1, GPIO::ModeInputPullup, VehicleIsInteriorOn },

    { 1, GPIO::ModeInputPullup, VehicleIsMenuButton1, 8 },
    { 1, GPIO::ModeInputPullup, VehicleIsMenuButton2, 9 },

    { 0, GPIO::ModeInputAnalog, VehicleRadiatorTemp },
	{ 0, GPIO::ModeInputAnalog, SensorAMPS1 },
	{ 0, GPIO::ModeInputAnalog, SensorHEGO1 },
	{ 0, GPIO::ModeInputAnalog, SensorHEGO2 },
	{ 0, GPIO::ModeInputAnalog, SensorBAR },
	{ 0, GPIO::ModeInputAnalog, SensorEGR },
	{ 0, GPIO::ModeInputAnalog, SensorVCC },
	{ 0, GPIO::ModeInputAnalog, SensorDEC },
	{ 0, GPIO::ModeInputAnalog, SensorTPS },
	{ 0, GPIO::ModeInputAnalog, SensorVSS },
	{ 0, GPIO::ModeInputAnalog, SensorECT },
	{ 0, GPIO::ModeInputAnalog, SensorMAF },
	{ 0, GPIO::ModeInputAnalog, SensorACT },
	{ 0, GPIO::ModeInputAnalog, VehicleFuelSender },
	{ 0 },
};

#elif CONFIG == 3

static GPIO::Def pins[] = {
    { 0, GPIO::ModeInputAnalog, VehicleFuelSender },
    { 0, GPIO::ModeOutput, VehicleIsTurningLeft },
    { 0, GPIO::ModeOutput, VehicleIsTurningRight },
    { 0, GPIO::ModeOutput, VehicleIsParkingOn },
    { 0, GPIO::ModeOutput, VehicleIsTransReverse },
	{ 0, GPIO::ModeOutput, SensorIsKeyOn },
    { 0, GPIO::ModeOutputPWM, CalcFuelPump },
	{ 0 },
};

#endif

void Vehicle::prompt_cb(Buffer &send, ShellEvent &se, void *data) {
	((Vehicle *)data)->sendStatus(send);
}

void Vehicle::sendStatus(Buffer &send) const {
	send.p1(F("vehicle"));
	send.json(F("fan1duty"), fan1.duty);
	send.json(F("fan2duty"), fan2.duty);
	send.p2();
}

static void setPins() {
	codes.clear();
	codes.set(20);
	codes.set(21);
	codes.set(50);
	codes.set(51);
	codes.set(52);
	codes.set(53);
	codes.set(DATA_PIN);
	codes.set(CLOCK_PIN);

	for (uint8_t i = 0; GPIO::MaxPins; i++) {
		GPIO::PinId id = ( GPIO::PinId)i;
		const GPIO::PinDef *pd = GPIO::getPinDef(id);

		if (pd->ext)
			codes.set(pd->ext);
	}

	int input = 24;
	int output = 2;
	int analog = 5;

	for (uint8_t i = 0; GPIO::MaxPins; i++) {
		GPIO::PinId id = ( GPIO::PinId)i;
		GPIO::PinDef *pd = (GPIO::PinDef *)GPIO::getPinDef(id);

		if (!pd->ext) {
			if (pd->mode & PinModeAnalog) {
				pd->ext = analog++ % 16;
			} else if (pd->mode & PinModeInput) {
				while (codes.isSet(input))
					input++;

				pd->ext = min(53, input);
			} else if (pd->mode & PinModeOutput) {
				while (codes.isSet(output))
					output++;

				pd->ext = min(53, output);
			}

			codes.set(pd->ext);

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

	float pct = (v - v1) / (float)(v2 - v1);
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

void updateCluster(uint8_t id, int v) {
#ifdef ARDUINO
	switch (id) {
		case VehicleIsTurningLeft:
			leds[PanelLeft] = v ? CRGB::Green : CRGB::Black;
			break;
		case VehicleIsTurningRight:
			leds[PanelRight] = v ? CRGB::Green : CRGB::Black;
			break;
		case VehicleIsHiBeamOn:
			leds[PanelHiBeam] = v ? CRGB::Blue : CRGB::Black;
			break;
		case VehicleIsGenAlert:
			leds[PanelGen] = v ? CRGB::Red : CRGB::Black;
			break;
		case VehicleIsOilAlert:
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

	const GPIO::PinDef *pi = GPIO::getPinDef((GPIO::PinId)inpin);

	if (pi && (pi->mode & PinModeInput)) {
		const uint16_t TESTLO = 400;
		const uint16_t TESTHI = 700;
		extern Buffer channel;

		switch (pi->getId()) {
			case GPIO::RadiatorTemp:
				setParamFloat(VehicleRadiatorTemp, scaleLinear(pi->getLast(), TESTLO, TESTHI, 150, 210, false));
				setTempGauge();
				break;
			case GPIO::AMPS1:
				setParamFloat(SensorAMPS1, scaleLinear(pi->getLast(), 512, 610, 500, 4000, false));
				break;
			case GPIO::IsMenuButton1:
				if (!pi->getLast())
					display.menuInput(channel, 0);
				break;
			case GPIO::IsMenuButton2:
				if (!pi->getLast())
					display.menuInput(channel, 1);
				break;
			case VehicleFuelSender:
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

	const GPIO::PinDef *pi = GPIO::getPinDef((GPIO::PinId)outpin);

	static const GPIO::PinDef *bl = 0;
	static const GPIO::PinDef *br = 0;

	if (pi && (pi->mode & PinModeOutput)) {
		switch (pi->getId()) {
			case CalcFan1:
			case CalcFan2:
				calcFanSpeed(now);
				break;
			case CalcEPAS1:
			case CalcEPAS2:
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
			case VehicleGauge1:
				v = scaleLinear(v, 0, 100, 0, 100, true);
				break;
			case CalcFan1:
			case CalcFan2:
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
	((Vehicle *)data)->checkVehicle(now);
	return 0;
}

uint16_t Vehicle::init() {
	brakeFlash = 0;

#ifdef ARDUINO
	LEDS.addLeds<WS2812,2,RGB>(leds,NUM_LEDS);
	LEDS.setBrightness(84);
#endif

	setPins();

	taskmgr.addTask(F("Vehicle"), runVehicle, this, 20);

	static ShellCallback callbacks[] = {
		{ F("vehicle"), prompt_cb, this },
	};

	shell.add(callbacks, ARRSIZE(callbacks));

	return sizeof(Vehicle) + sizeof(callbacks);
}
