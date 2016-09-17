#include "Decoder.h"
#include "Metrics.h"
#include "Broker.h"

void Decoder::sendStatus(Buffer &send) volatile {
	send.p1(F("decoder"));
	send.json(F("edges"), edges);
	send.json(F("tdc"), tdc);
	send.json(F("old"), old);
	send.json(F("idx"), idx);
	send.json(F("total"), getTicks());
	send.json(F("rpm"), getRPM());
	send.json(F("invalid"), !isValid());
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

static void dstatus(Buffer &send, BrokerEvent &be, void *data) {
	((Decoder *)data)->sendStatus(send);
}

static void d0(Buffer &send, BrokerEvent &be, void *data) {
	((Decoder *)data)->sendStatus(send);
}

static void d1(Buffer &send, BrokerEvent &be, void *data) {
	((Decoder *)data)->sendList(send);
}

void Decoder::init() volatile {
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

	broker.add(dstatus, (void *)this, F("dstatus"));
	broker.add(d0, (void *)this, F("d0"));
	broker.add(d1, (void *)this, F("d1"));
}

uint16_t Decoder::mem(bool alloced) volatile {
	return 0;
}