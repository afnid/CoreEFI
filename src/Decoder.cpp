#include "Decoder.h"
#include "Metrics.h"
#include "Shell.h"

void Decoder::sendStatus(Buffer &send) volatile {
	send.p1(F("decoder"));
	send.json(F("edges"), edges);
	send.json(F("tdc"), tdc);
	send.json(F("old"), old, false);
	send.json(F("idx"), idx);
	send.json(F("total"), getTicks());
	send.json(F("rpm"), getRPM());
	send.json(F("invalid"), !isValid(), false);
	sendHist(send, hist, HistMax);
	send.p2();

	old = 0;
}

void Decoder::sendList(Buffer &send) volatile {
	for (uint8_t i = 0; i < edges; i++) {
		uint8_t j = index(tdc + i);
		send.p1(F("pulse"));
		send.json(F("i"), i);
		send.json(F("id"), j);
		send.json(F("pw"), pulses[j]);
		send.json(F("hi"), ishi(j));
		send.p2();
	}
}

static void dstatus(Buffer &send, ShellEvent &se, void *data) {
	((Decoder *)data)->sendStatus(send);
}

static void d0(Buffer &send, ShellEvent &se, void *data) {
	((Decoder *)data)->sendStatus(send);
}

static void d1(Buffer &send, ShellEvent &se, void *data) {
	((Decoder *)data)->sendList(send);
}

uint16_t Decoder::init() volatile {
	valid = 0;
	total = 0;
	tdcts = 0;

	idx = 0;
	tdc = 0;
	last = 0;
	sum = 0;
	edges = 0;
	teeth = 0;
	key = -1;

	for (uint8_t i = 0; i < MaxEncoderTeeth * 2; i++)
		pulses[i] = 0;

	refresh(0);

	static ShellCallback callbacks[] = {
		{ F("dstatus"), dstatus, (Decoder *)this },
		{ F("d0"), d0, (Decoder *)this },
		{ F("d1"), d1, (Decoder *)this },
	};

	shell.add(callbacks, ARRSIZE(callbacks));

	return sizeof(Decoder) + sizeof(callbacks);
}
