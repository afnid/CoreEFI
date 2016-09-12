#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Prompt.h"
#include "Params.h"
#include "Decoder.h"

static void dstatus(void *data) {
	Decoder *d = (Decoder *)data;
	d->sendStatus();
}

static void d0(void *data) {
	((Decoder *)data)->sendStatus();
}

static void d1(void *data) {
	((Decoder *)data)->sendList();
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
