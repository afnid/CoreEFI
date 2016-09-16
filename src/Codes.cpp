#include "Codes.h"
#include "Shell.h"

static void codescb(Buffer &send, ShellEvent &se, void *data) {
	((Codes *)data)->send(send);
}

uint16_t Codes::init() {
	bzero(codes, sizeof(codes));

	shell.add(codescb, this, F("codes"), 0);

	return sizeof(Codes);
}

