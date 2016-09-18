// copyright to me, released under GPL V3

#ifndef _Stream_h_
#define _Stream_h_

#include "Buffer.h"

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

class Streamx {
	uint8_t level;

public:

	uint8_t getLevel();
	uint8_t setLevel(uint8_t level);
};

#endif
