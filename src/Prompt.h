#ifndef _Prompt_h_
#define _Prompt_h_

#include "Buffer.h"

typedef struct _PromptCallback {
	const channel_t *key;
	void (*callback)(Buffer &send, void *data);
	void *data;
	const channel_t *desc;
	struct _PromptCallback *next;
} PromptCallback;

uint16_t initPrompt();

void addPromptCallbacks(PromptCallback *callbacks, uint8_t ncallbacks); // need to be static

#endif
