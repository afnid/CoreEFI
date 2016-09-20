#include "Vehicle.h"
#include "Hardware.h"
#include "Display.h"
#include "Params.h"
#include "GPIO.h"
#include "Tasks.h"

#ifdef ARDUINO
#include "FastLED.h"
#endif

#define DATA_PIN 12
#define CLOCK_PIN 13

enum {
	PanelOil, PanelGen, PanelHiBeam, PanelLeft, PanelRight, PanelBacklight1, PanelBacklight2, PanelBacklight3, PanelBacklight4, NUM_LEDS,
};

#ifdef ARDUINO
CRGB leds[NUM_LEDS];
#endif

void Vehicle::sendStatus(Buffer &send, BrokerEvent &be) const {
	if (be.isMatch(send, F("f"))) {
		fan1.json(F("fan1"), send);
		fan2.json(F("fan2"), send);
		send.p2();
	} else if (be.isMatch(send, F("s"))) {
		odb1.json(F("odb1"), send);
		epas.json(F("epas"), send);
		json(send);
	} else
		send.nl("fans|status");
}

static float scaleLinear(uint16_t v, uint16_t v1, uint16_t v2, float o1, float o2, bool clamp) {
	if (clamp && v <= v1)
		return o1;
	if (clamp && v >= v2)
		return o2;

	float pct = (v - v1) / (float) (v2 - v1);
	return o1 + (o2 - o1) * pct;
}

void Vehicle::json(Buffer &send) const {
	send.p1(F("vehicle"));
	send.json(F("flash"), brakeFlash);
	send.json(F("turn"), turning);
	send.p2();
}

void Coded::init() {
	id = MaxPins;
	state = 0;
	value = 0;
	edge_lo = 0;
	edge_hi = 0;
}

void Coded::json(const flash_t *name, Buffer &send) const {
	send.p1(name);
	send.json(F("id"), (uint8_t)id);
	send.json(F("value"), value);
	send.json(F("state"), state);
	send.p2(false);
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

void Pulsed::json(const flash_t *name, Buffer &send) const {
	send.p1(name);
	send.json("duty", duty);
	send.json("calced", tdiff32(millis(), calced));
	send.p2(false);
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
	const GPIO::PinDef *pd = gpio.getPinDef(PinIsKeyOn);
	return gpio.isPinSet(PinIsKeyOn) ? pd->ms() / 1000 : 0;
}

uint32_t Vehicle::getOffSeconds() const {
	const GPIO::PinDef *pd = gpio.getPinDef(PinIsKeyOn);
	return !gpio.isPinSet(PinIsKeyOn) ? pd->ms() / 1000 : 0;
}

bool Vehicle::isRunning() {
	return getParamUnsigned(SensorRPM) > 200;
}

void Vehicle::calcSteeringAssist(uint32_t now) {
	uint16_t v = isRunning() ? 20 : 0;
	setParamUnsigned(CalcEPAS1, v);

	uint16_t duty = 0;

	if (epas.duty && getParamUnsigned(TimeRunSeconds) > 5) {
		uint16_t mph = getParamFloat(SensorVSS);

		if (mph <= 10)
			duty = clamp(duty, 0, 100 - mph * 10);
	}

	epas.ramp(now, duty, 100);

	setParamUnsigned(CalcEPAS2, epas.duty);
}

void Vehicle::calcFanSpeed(uint32_t now) {
	float mph = getParamFloat(SensorVSS);
	float temp = getParamFloat(VehicleRadiatorTemp);
	uint16_t duty = 0;

	if (mph < 50 && temp >= 165) {
		uint16_t max = gpio.isPinSet(PinIsClimateOn) ? 185 : 195;
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
		case PinIsSignalLeft:
		leds[PanelLeft] = v ? CRGB::Green : CRGB::Black;
		break;
		case PinIsSignalRight:
		leds[PanelRight] = v ? CRGB::Green : CRGB::Black;
		break;
		case PinIsHiBeamOn:
		leds[PanelHiBeam] = v ? CRGB::Blue : CRGB::Black;
		break;
		case PinIsBrakeOn: //IsGenAlert:
		leds[PanelGen] = v ? CRGB::Red : CRGB::Black;
		break;
		case PinIsParkingOn://IsOilAlert:
		leds[PanelOil] = v ? CRGB::Red : CRGB::Black;
		break;
	}

	FastLED.show();
#endif
}

static void setBrakeLights() {
	uint16_t brakes = gpio.isPinSet(PinIsBrakeOn);
	uint16_t lt = gpio.isPinSet(PinIsSignalLeft);
	uint16_t rt = gpio.isPinSet(PinIsSignalRight);

	gpio.setPin(PinRelayBrakeLightLeft, brakes || lt);
	gpio.setPin(PinRelayBrakeLightRight, brakes || rt);
}

static void setParking() {
	uint16_t hi = gpio.isPinSet(PinIsHiBeamOn);
	uint16_t lo = gpio.isPinSet(PinIsLoBeamOn);

	if (hi || lo)
		gpio.setPin(PinIsParkingOn, true);
}

static void setTempGauge() {
	if (gpio.isPinSet(PinIsKeyOn)) {
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
		case PinAnalogRadiatorTemp:
			setParamFloat(VehicleRadiatorTemp, scaleLinear(pd->getLast(), TESTLO, TESTHI, 150, 210, false));
			setTempGauge();
			break;
		case PinAnalogAMPS1:
			setParamFloat(SensorAMPS1, scaleLinear(pd->getLast(), 512, 610, 500, 4000, false));
			break;
		case PinIsMenuButton1:
			if (pd->getLast())
				display.menuInput(0);
			break;
		case PinIsMenuButton2:
			if (!pd->getLast())
				display.menuInput(1);
			break;
		case PinAnalogFuel:
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
		case PinRelayFan1:
		case PinRelayFan2:
			calcFanSpeed(now);
			break;
		case PinRelayEPAS1:
		case PinRelayEPAS2:
			calcSteeringAssist(now);
			break;
		case PinIsParkingOn:
			setParking();
			break;
		case PinRelayBrakeLightLeft:
			if (turning)
				return;
			setBrakeLights();
			bl = pd;
			break;
		case PinRelayBrakeLightRight:
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
		case PinRelayGauge1:
			v = scaleLinear(v, 0, 100, 0, 100, true);
			break;
		case PinRelayFan1:
		case PinRelayFan2:
			v = v <= 0 ? 0 : scaleLinear(v, 0, 100, 80, 255, true);
			break;
		case PinRelayEPAS2:
			v = scaleLinear(v, 0, 100, 0, 100, true);
			break;
		case PinIsSignalLeft:
		case PinIsSignalRight:
			turning = v != 0;

			setBrakeLights();

			if (v) {
				if (pd->getLast() && pd->ms() >= 600)
					v = 0;
				else if (!pd->getLast() && pd->ms() >= 1200)
					v = 1;
				else
					v = pd->getLast();

				if (bl && pd->getId() == PinIsSignalLeft)
					gpio.setPin(bl->getId(), v);
				//if (br && pd->getId() == VehicleIsTurningRight)
				//br->writePin(v);
			}

			updateCluster(pd->getId(), v);
			break;
		case PinRelayBrakeLightLeft:
		case PinRelayBrakeLightRight:
			if (gpio.isPinSet(PinIsBrakeOn)) {
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
		if (!gpio.isPinSet(PinIsParkingOn) && getParamUnsigned(TimeRunSeconds) > 3)
			gpio.setPin(PinIsParkingOn, true);
		if (!gpio.isPinSet(PinIsLoBeamOn) && getParamUnsigned(TimeRunSeconds) > 5)
			gpio.setPin(PinIsLoBeamOn, true);
	} else {
		if (gpio.isPinSet(PinIsLoBeamOn) && getParamUnsigned(TimeRunSeconds) > 3)
			gpio.setPin(PinIsLoBeamOn, false);
		if (gpio.isPinSet(PinIsParkingOn) && getParamUnsigned(TimeRunSeconds) > 5)
			gpio.setPin(PinIsParkingOn, false);
	}

	for (int i = 0; i < 10; i++)
		serviceOutput(now);

	/*
	 odb1.service(now);
	 */
}

uint32_t Vehicle::taskcb(uint32_t now, void *data) {
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

	taskmgr.addTask(F("Vehicle"), taskcb, this, 20);
	broker.add(brokercb, this, F("c"));
}

uint16_t Vehicle::mem(bool alloced) {
	return alloced ? 0 : 0;
}
