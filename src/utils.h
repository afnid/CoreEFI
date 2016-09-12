#ifndef _utils_h_
#define _utils_h_

#ifdef ARDUINO
#include <Arduino.h>
#define bzero(x, n)	memset(x, 0, n)
#else
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#endif

int mysprintf(char *buf, const char *fmt, ...);
uint32_t hextol(const char *s);

// 126, page 18.6
#define DIRLEN 128
#define ATLEN (DIRLEN + 40)

const char *makedir(char *buf, const char *name);
const char *join(char ch, char *dst, const char *src);
const char *joindir(char *dst, const char *src);

void vbzero(volatile void *ptr, size_t len);
void vmemcpy(volatile uint8_t *dst, volatile const uint8_t *src, size_t len);
int16_t vmemcmp(volatile const uint8_t *dst, volatile const uint8_t *src, size_t len);

uint32_t millis();
uint32_t micros();
uint32_t ticks();

void delayus(uint32_t now, uint32_t interval);
void delayus(uint16_t us);
void delayms(uint16_t ms);

uint32_t getBuildVersion();
bool startsWith(const char *haystack, const char *needle);
uint16_t tokenize(char *msg, char **tokens, uint16_t max, const char *patterns);

uint8_t *loadFile(const char *path, int32_t *len);
bool saveFile(const char *path, const void *buf, uint32_t len);

uint32_t crc32(const uint8_t *buf, int32_t len) ;
uint16_t crc16(const void * ptr, uint16_t len);
uint8_t crc8(const volatile uint8_t *buf, uint8_t len);
uint8_t crapcrc(const volatile uint8_t *buf, uint8_t len);

#define ARRSIZE(x)	(sizeof(x) / sizeof(x[0]))
#define tdiff32(a, b)	((int32_t)((uint32_t)(a) - (uint32_t)(b)))
#define tdiff16(a, b)	((int16_t)((uint16_t)(a) - (uint16_t)(b)))

#ifndef min
#define min(a, b)	(a <= b ? a : b)
#endif

#ifndef max
#define max(a, b)	(a >= b ? a : b)
#endif

#include <stdarg.h>
extern void _myerror(const char *path, uint16_t line, ...);
#define myerror(...)	_myerror(PATH, __LINE__, ## __VA_ARGS__)

#ifndef EXTERN
#define EXTERN extern
#endif

#endif
