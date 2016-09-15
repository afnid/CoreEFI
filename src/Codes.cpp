#include "Codes.h"
#include "Shell.h"

static void codescb(Buffer &send, ShellEvent &se, void *data) {
	((Codes *)data)->send(send);
}

uint16_t Codes::init() {
	bzero(codes, sizeof(codes));

	static ShellCallback callbacks[] = {
		{ F("codes"), codescb, this },
	};

	shell.add(callbacks, ARRSIZE(callbacks));

	return sizeof(Codes);
}

