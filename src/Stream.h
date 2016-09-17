// copyright to me, released under GPL V3

#ifndef _Stream_h_
#define _Stream_h_

#include "Buffer.h"

#ifndef ARDUINO

typedef char flash_t;

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

void _log(uint8_t level, const flash_t *file, int line, const flash_t *fmt, ...);

#define LOG_CALL(level, fmt, ...) _log(level, F(__FILE__), __LINE__, F(fmt), ## __VA_ARGS__);
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

#define flash_t __FlashStringHelper
#define _printOverload(fmt, type) int print(type v) { return out->print(v); }
#define sendNameVal(t) void send(const __FlashStringHelper *label, t v, bool verbose = true) { if (verbose || v) { name(label); out->print(v); }}
#define sendIdVal(t) void send(uint8_t id, t v, bool verbose = true) { if (verbose || v) { name(id); out->print(v); }}

#else

#define _printOverloadf(f, t) int print(t v) { return printf(f, (double)v); }
#define _printOverload(f, t) int print(t v) { return printf(f, v); }
#define sendNameVal(t) void send(const flash_t *label, t v, bool verbose = true) { if (verbose || v) { name(label); print(v); }}
#define sendIdVal(t) void send(uint8_t id, t v, bool verbose = true) { if (verbose || v) { name(id); print(v); }}

#endif



class Stream {
	uint8_t level;

public:

	uint8_t getLevel();
	uint8_t setLevel(uint8_t level);
};

extern Buffer channel;

#endif
