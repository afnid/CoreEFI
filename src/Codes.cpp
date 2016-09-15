#include "System.h"
#include "Buffer.h"
#include "Prompt.h"
#include "Params.h"
#include "Codes.h"

static void codescb(Buffer &send, void *data) {
	((Codes *)data)->send(send);
}

uint16_t Codes::init() {
	myzero(codes, sizeof(codes));

	static PromptCallback callbacks[] = {
		{ F("codes"), codescb, this },
	};

	addPromptCallbacks(callbacks, ARRSIZE(callbacks));

	return sizeof(Codes);
}

