// copyright to me, released under GPL V3

#ifndef ARDUINO

#include <stdio.h>

typedef char channel_t;

class Print {
public:
	int println(const char *s) {
		return puts(s);
	}

	bool available() {
		extern bool available(int fd);
		return available(0);
	}

	char read() {
		extern char readch(int fd);
		return readch(0);
	}

	void flush() {
		fflush(stdout);
	}

	void begin(int baud) {
	}
};

#endif



#include <stdarg.h>

enum {
	LOG_FINE,
	LOG_INFO,
	LOG_WARNING,
	LOG_SEVERE
};

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_FINE
#endif

#define LOG_CALL(level, fmt, ...) channel._log(level, F(__FILE__), __LINE__, F(fmt), ## __VA_ARGS__);
#define TLOG_CALL(truth, level, fmt, ...) if (!(truth)) LOG_CALL(level, fmt, ## __VA_ARGS__)

#if LOG_LEVEL <= LOG_FINE
#define logFine(fmt, ...) LOG_CALL(LOG_FINE, fmt, ## __VA_ARGS__)
#define tlogFine(fmt, ...) TLOG_CALL(LOG_FINE, fmt, ## __VA_ARGS__)
#else
#define logFine(fmt, ...)
#define tlogFine(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_INFO
#define logInfo(fmt, ...) LOG_CALL(LOG_INFO, fmt, ## __VA_ARGS__)
#define tlogInfo(fmt, ...) TLOG_CALL(LOG_INFO, fmt, ## __VA_ARGS__)
#else
#define logInfo(fmt, ...)
#define tlogInfo(fmt, ...)
#endif

#if LOG_LEVEL <= LOG_WARNING
#define logWarning(fmt, ...) LOG_CALL(LOG_WARNING, fmt, ## __VA_ARGS__)
#else
#define logWarning(fmt, ...)
#endif

#define logSevere(fmt, ...) LOG_CALL(LOG_SEVERE, fmt, ## __VA_ARGS__)
#define tlogSevere(fmt, ...) TLOG_CALL(LOG_SEVERE, fmt, ## __VA_ARGS__)



#ifdef ARDUINO

#include <Print.h>

#define channel_t __FlashStringHelper
#define _printOverload(fmt, type) int print(type v) { return out->print(v); }
#define sendNameVal(f, t) void send(const __FlashStringHelper *label, t v, bool verbose = true) { if (verbose || v) { name(label); out->print(v); }}
#define sendIdVal(f, t) void send(uint8_t id, t v, bool verbose = true) { if (verbose || v) { name(id); out->print(v); }}

#else

#define _printOverload(f, t) int print(t v) { return printf(f, v); }
#define sendNameVal(f, t) void send(const channel_t *label, t v, bool verbose = true) { if (verbose || v) { name(label); printf(f, v); }}
#define sendIdVal(f, t) void send(uint8_t id, t v, bool verbose = true) { if (verbose || v) { name(id); printf(f, v); }}

#endif



class Channel {
#ifdef ARDUINO
	int println() {
		return out->println();
	}

	int putstr(const __FlashStringHelper *s) {
		return out->print(s);
	}

	int eputstr(const char *s) {
		return out->print(s);
	}

	int eprintln(const char *s) {
		return out->println(s);
	}

	int eprint(char c) {
		return out->print(c);
	}

#else
	int println() {
		putchar('\n');
		fflush(stdout);
		return 1;
	}

	int putstr(const char *s) {
		return printf("%s", s);
	}

	int eputstr(const char *s) {
		return fprintf(stderr, "%s", s);
	}

	int eprintln(const char *s) {
		return fprintf(stderr, "%s\n", s);
	}

	int eprint(char c) {
		return fprintf(stderr, "%c", c);
	}
#endif

	_printOverload("%c", char)
	_printOverload("%d", int16_t)
	_printOverload("%u", uint16_t)
#ifdef STM32
	_printOverload("%ld", int32_t)
	_printOverload("%lu", uint32_t)
#else
	_printOverload("%d", int32_t)
	_printOverload("%u", uint32_t)
#endif
	_printOverload("%.2f", double)

	char last;

	void name(const channel_t *s);
	void name(uint8_t id);
	uint8_t level;
	Print *out;

public:

	Channel(Print &print) {
		out = &print;
		level = LOG_LEVEL;
		last = 0;
	}

	void send(const channel_t *s) {
		putstr(s);
		last = 0;
	}

	void p1(const channel_t *s);
	void p2();

	void nl() {
		println();
		last = 0;
	}

	sendNameVal("%d", int8_t)
	sendNameVal("%u", uint8_t)
	sendNameVal("%d", int16_t)
	sendNameVal("%u", uint16_t)
#ifdef STM32
	sendNameVal("%ld", int32_t)
	sendNameVal("%lu", uint32_t)
#else
	sendNameVal("%d", int32_t)
	sendNameVal("%u", uint32_t)
#endif
	sendNameVal("%.2f", double)
	sendNameVal("%d", char)
	sendNameVal("%d", bool)

	sendIdVal("%d", int16_t)
	sendIdVal("%u", uint16_t)
	sendIdVal("%g", double)
	sendIdVal("%d", uint8_t)

	uint8_t getLevel();
	uint8_t setLevel(uint8_t level);

	void _log(uint8_t level, const channel_t *file, int line, const channel_t *fmt, ...);
};

extern Channel channel;
