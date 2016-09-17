#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ADDR 0x27

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4); // a4, a5
#endif

#include "Display.h"
#include "System.h"
#include "Buffer.h"
#include "Params.h"
#include "Hardware.h"
#include "Broker.h"

#include "Tasks.h"
#include "GPIO.h"

static char *concatch(char *s, char ch) {
	s += strlen(s);
	*s++ = ch;
	*s = 0;
	return s;
}

static int getdiv(int v) {
	v = abs(v);

	if (v < 10)
		return 1;
	if (v < 100)
		return 10;
	if (v < 1000)
		return 100;
	if (v < 10000)
		return 1000;

	return 10000;
}

static char *itoa(char *s, int v) {
	s += strlen(s);
	mysprintf(s, "%d", v);
	return s + strlen(s);

	int div = 0;

	do {
		div = getdiv(v);
		int i = v / div;
		s = concatch(s, '0' + i);
		v -= i * div;
	} while (div > 1);

	return s;
}

#if 0
static char *concatage(char *s, uint32_t ms)
{
	long ss = ms / 1000;
	long hh = ss / 3600;
	ss -= hh * 3600;
	long mm = ss / 60;
	ss -= mm * 60;

	s += strlen(s);
	s = itoa(s, hh);
	s = concatch(s, ':');
	if (mm < 10)
		s = concatch(s, '0');
	s = itoa(s, mm);
	s = concatch(s, ':');
	if (ss < 10)
		s = concatch(s, '0');
	s = itoa(s, ss);
	return s;
}
#endif

static char *addlabel(char *s, const char *label, bool space = true) {
	if (space)
		s = concatch(s, ' ');
	strcat(s, label);
	return s + strlen(s);
}

static char *concat(char *s, const char *label, uint16_t val, bool space = true) {
	if (val || !val) {
		s = addlabel(s, label, space);
		s = concatch(s, ' ');
		s = itoa(s, val);
	}

	return s + strlen(s);
}

#if 0
static char *concat(char *s, const char *label, uint32_t val, uint32_t now)
{
	if (val) {
		if (*s)
			s = concatch(s, ' ');
		strcat(s, label);
		s = concatch(s, ' ');
		s = concatage(s, tdiff32(now, val));
	}

	return s + strlen(s);
}
#endif

static char *formatLast(char *buf, GPIO::PinId id) {
	char *s = buf;
	*s = 0;

	const GPIO::PinDef *p = GPIO::getPinDef(id);

	if (p && p->ms() < 5000) {
		itoa(s, p->ext);
		strcat(s, " ");
		itoa(s, p->mode);
		strcat(s, " ");
		itoa(s, p->getLast());
		strcat(s, " ");

		if (p->name)
			strcat(s, p->name);
	}

	return buf;
}

static char *formatTemp(char *buf) {
	char *s = buf;
	*s = 0;

	s = addlabel(s, "Temp:");
	s = itoa(s, getParamFloat(VehicleRadiatorTemp));
	s = concatch(s, (char)223);
	s = concatch(s, '/');
	s = itoa(s, getParamUnsigned(SensorAMPS1));
	//s = itoa(s, getParamFloat(SensorECT));
	s = concatch(s, (char)223);

	return buf;
}

static char *formatPWM(char *buf) {
	char *s = buf;
	*s = 0;

	s = concat(s, "F1", getParamUnsigned(CalcFan1), false);
	s = concat(s, "F2", getParamUnsigned(CalcFan2));
	s = concat(s, "E1", getParamUnsigned(CalcEPAS1));
	s = concat(s, "E2", getParamUnsigned(CalcEPAS2));

	return buf;
}

static char *formatGauges(char *buf) {
	char *s = buf;
	*s = 0;

	s = concat(s, "G1", getParamUnsigned(VehicleGauge1), false);
	s = concat(s, "G2", getParamUnsigned(VehicleGauge2));

	return buf;
}

static char *formatAnalog(char *buf, uint8_t pin) {
	char *s = buf;
	*s = 0;

	strcpy(s, "A");
	s = itoa(s, pin);

	for (uint8_t i = 0; i < 4; i++) {
		s = concatch(s, ' ');
		// XX s = itoa(s, analogRead(pin++));
	}

	return buf;
}

static char *formatVolts(char *buf) {
	char *s = buf;
	*s = 0;

	s = concat(buf, "VCC", Hardware::getBattery(), false);

	return buf;
}

void Display::showLine(int line, char *buf) {
	int n = strlen(buf);

	while (n < 20)
		buf[n++] = ' ';

	buf[20] = 0;

	if (strcmp(display[line], buf)) {
#ifdef ARDUINO
		lcd.setCursor(0, line);
		lcd.print(buf);
#endif

		strcpy(display[line], buf);
	}
}

void Display::showDisplay(uint32_t now) {
	char buf[40];

	if (row <= 0) {
		showLine(row & 1, formatLast(buf, (GPIO::PinId)(row & 1)));
	} else {
		switch (menu) {
			case 0:
				showLine(1, formatAnalog(buf, 5));
				showLine(2, formatTemp(buf));
				showLine(3, formatGauges(buf));
				break;
			case 1:
				showLine(1, formatAnalog(buf, 5));
				showLine(2, formatTemp(buf));
				showLine(3, formatPWM(buf));
				break;
			case 2:
				showLine(1, formatAnalog(buf, 5));
				showLine(2, formatTemp(buf));
				showLine(3, formatVolts(buf));
				break;
		}
	}

	++row;
	row &= 3;
}

void Display::menuInput(Buffer &send, uint8_t i) {
	if (i && menu < 2)
		menu++;
	if (!i && menu > 0)
		menu--;
}

static uint32_t runDisplay(uint32_t now, void *data) {
	((Display *)data)->showDisplay(now);
	return 0;
}

static void menucb0(Buffer &send, BrokerEvent &be, void *data) {
	((Display *)data)->menuInput(send, 0);
}

static void menucb1(Buffer &send, BrokerEvent &be, void *data) {
	((Display *)data)->menuInput(send, 1);
}

uint16_t Display::init() {
	memset(display, 0, sizeof(display));

	menu = 0;
	row = 0;

#ifdef ARDUINO
	lcd.init();
	lcd.setBacklight(250);
	lcd.setContrast(20);
#endif

	broker.add(menucb0, this, F("m0"));
	broker.add(menucb1, this, F("m1"));

	taskmgr.addTask(F("Display"), runDisplay, this, 500);

	return sizeof(Display);
}
