typedef struct _PromptCallback {
	const channel_t *key;
	void (*callback)(void *data);
	void *data;
	const channel_t *desc;
	struct _PromptCallback *next;
} PromptCallback;

uint16_t initPrompt();

void addPromptCallbacks(PromptCallback *callbacks, uint8_t ncallbacks); // need to be static
