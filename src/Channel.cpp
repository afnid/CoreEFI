#include "System.h"
#include "Channel.h"

static const char SEP = '\t';

void Channel::name(const channel_t *label) {
	if (last != '{')
		print(SEP);

	int n = putstr(label);
	print('=');

	while (n++ < 7)
		print(' ');

	last = '=';
}

void Channel::name(uint8_t id) {
	if (last != '{')
		print(SEP);
	print((uint16_t)id);
	print('=');
	last = '=';
}

void Channel::p1(const channel_t *label) {
	if (last)
		print(SEP);

	putstr(label);
	print(':');
	print('{');
	last = '{';
}

void Channel::p2() {
	print('}');
	last = '}';
}

#include <string.h>

static void pgmstr(char *buf, uint16_t len, const channel_t *pgm) {
#ifdef ARDUINO
	char ch;
	int i = 0;

	//while((ch = pgm_read_byte(pgm + i)) && i < len - 1)
	//buf[i++] = ch;

	buf[i] = 0;
#else
	strcpy(buf, pgm);
	assert(strlen(buf) < len);
#endif
}

void Channel::_log(uint8_t level, const channel_t *file, int line, const channel_t *fmt, ...) {
	if (level >= this->level) {
		//showms();

		eprint("FIWE"[level]);
		eprint(' ');

		char str[50];

		if (file) {
			pgmstr(str, sizeof(str), file);
			char *f = strrchr(str, '/');
			eputstr(f ? f + 1 : str);
		}

		if (line) {
			eprint(':');
			eprint(line);
			eprint(' ');
		}

		/*
		 if (errline) {
		 pgmstr(str, sizeof(str), errfile);
		 char *f = strrchr(str, '/');
		 out->print(f ? f + 1 : str);
		 out->print(':');
		 out->print(errline);
		 out->print(' ');
		 }
		 */

		pgmstr(str, sizeof(str), fmt);
		char buf[80];

		//for (int n = strlen(buf) + 1; n < 30; n++)
		//out->print(' ');

		va_list va;
		va_start(va, fmt);
		vsnprintf(buf, sizeof(buf) - 1, str, va);

		eprint(' ');
		eprintln(buf);
	}
}

uint8_t Channel::setLevel(uint8_t level) {
	uint8_t old = this->level;
	this->level = level;
	this->level = max(this->level, LOG_FINE);
	this->level = min(this->level, LOG_WARNING);
	return old;
}


