#include "System.h"
#include "Channel.h"
#include "Prompt.h"
#include "Params.h"
#include "Codes.h"

static void codescb(void *data) {
	((Codes *)data)->send();
}

uint16_t Codes::init() {
	myzero(codes, sizeof(codes));

	static PromptCallback callbacks[] = {
		{ F("codes"), codescb, this },
	};

	addPromptCallbacks(callbacks, ARRSIZE(callbacks));

	return sizeof(Codes);
}

