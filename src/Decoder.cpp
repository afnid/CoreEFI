#include "System.h"
#include "Buffer.h"
#include "Metrics.h"
#include "Prompt.h"
#include "Params.h"
#include "Decoder.h"

static void dstatus(Buffer &send, void *data) {
	Decoder *d = (Decoder *)data;
	d->sendStatus(send);
}

static void d0(Buffer &send, void *data) {
	((Decoder *)data)->sendStatus(send);
}

static void d1(Buffer &send, void *data) {
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

	static PromptCallback callbacks[] = {
		{ F("dstatus"), dstatus, (Decoder *)this },
		{ F("d0"), d0, (Decoder *)this },
		{ F("d1"), d1, (Decoder *)this },
	};

	addPromptCallbacks(callbacks, ARRSIZE(callbacks));

	return sizeof(Decoder) + sizeof(callbacks);
}
