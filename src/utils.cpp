#include "utils.h"

uint32_t getBuildVersion() {
    static uint32_t version = 0;

    if (!version) {
        version = 1;

        char tmp[30];
        mysprintf(tmp, "%s %s", __DATE__, __TIME__);
        char *tokens[7];
        int ntokens = tokenize(tmp, tokens, 7, "\" :");

        if (ntokens == 6) {
            const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
            const char *month = strstr(months, tokens[0]);
            int mon = month ? 1 + (month - months) / 3 : 0;

            uint16_t d = atoi(tokens[1]);
            uint16_t y = atoi(tokens[2]);
            uint16_t h = atoi(tokens[3]);
            uint16_t m = atoi(tokens[4]);

            char buf[20];
            mysprintf(buf, "%d%02d%02d%02d%02d", y % 10, mon, d, h, (m / 5) * 5);
            version = atol(buf);
        }
    }

    return version;
}


/***
 * CRC Table and code from Maxim AN 162:
 *  http://www.maximintegrated.com/en/app-notes/index.mvp/id/162
 */
static uint8_t calc_crc8(uint8_t x, uint8_t crc) {
	static const uint8_t lookup[] = { 0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65, 157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220, 35, 125, 159, 193,
			66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98, 190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255, 70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229,
			187, 89, 7, 219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154, 101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36, 248, 166, 68, 26, 153, 199,
			37, 123, 58, 100, 134, 216, 91, 5, 231, 185, 140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205, 17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14,
			80, 175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238, 50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115, 202, 148, 118, 40, 171, 245, 23, 73,
			8, 86, 180, 234, 105, 55, 213, 139, 87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22, 233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168, 116,
			42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53 };

	return lookup[crc ^ x];
}

uint8_t crc8(const volatile uint8_t *buf, uint8_t len) {
	uint8_t crc = 0;

	while (len--)
		crc = calc_crc8(*buf++, crc);

	return crc;
}

uint8_t crapcrc(const volatile uint8_t *buf, uint8_t len) {
	uint8_t crc = 0;

	while (len--)
		crc += *buf++;

	return crc;
}

uint16_t crc16(const void * ptr, uint16_t len, uint16_t crc) {
	if (ptr) {
		const uint8_t *buf = (uint8_t*) ptr;

		while (len--) {
			uint8_t x = crc >> 8 ^ *buf++;
			x ^= x >> 4;
			crc = (crc << 8) ^ ((uint16_t) (x << 12)) ^ ((uint16_t) (x << 5)) ^ ((uint16_t) x);
		}
	}

	return crc;
}

uint16_t crc16(const void * ptr, uint16_t len) {
	return crc16(ptr, len, 0xffff);
}

uint32_t crc32(const uint8_t *buf, int32_t len) {
	uint32_t crc = ~0L;

	while (len--) {
		uint32_t byte = *buf++;
		crc = crc ^ byte;

		for (int j = 7; j >= 0; j--) { // Do eight times.
			uint32_t mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}

	return ~crc;
}

bool startsWith(const char *haystack, const char *needle) {
	return !strncmp(haystack, needle, strlen(needle));
}

uint16_t tokenize(char *msg, char **tokens, uint16_t max, const char *patterns) {
	bzero(tokens, sizeof(tokens[0]) * max);

	uint16_t ntokens = 0;
	char *token;
	char *i = 0;

	while ((token = strtok_r(msg, patterns, &i))) {
		if (ntokens + 1 < max)
			tokens[ntokens++] = token;

		msg = 0;
	}

	return ntokens;
}

void vbzero(volatile void *ptr, size_t len) {
	volatile uint8_t *p = (volatile uint8_t *) ptr;

	while (len--)
		*p++ = 0;
}

void vmemcpy(volatile uint8_t *dst, volatile const uint8_t *src, size_t len) {
	while (len--)
		*dst++ = *src++;
}

int16_t vmemcmp(volatile const uint8_t *dst, volatile const uint8_t *src, size_t len) {
	while (len--) {
		if (*dst != *src)
			return *dst - *src;
		*dst++;
		*src++;
	}

	return 0;
}

void delayus(uint32_t now, uint32_t us) {
	if (us)
		while (tdiff32(micros(), now) < (int32_t) us)
			;
}

void delayus(uint16_t us) {
	uint32_t now = micros();
	delayus(now, us);
}

void delayms(uint16_t ms) {
	delayus(ms * 1000);
}

int unhex(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return 0;
}

uint32_t hextol(const char *s) {
	unsigned long x = 0;

	if (startsWith(s, "0x"))
		s += 2;

	while (s && *s) {
		x <<= 4;
		x |= unhex(*s++);
	}

	return x;
}

uint32_t hextoi(const char *buf) {
    uint32_t i = 0;

    if (startsWith(buf, "0x"))
        buf += 2;

    while (*buf) {
        i <<= 4;
        i |= unhex(*buf++);
    }

    return i;
}

uint64_t hextoi64(const char *buf) {
    uint64_t i = 0;

    if (startsWith(buf, "0x"))
        buf += 2;

    while (*buf) {
        i <<= 4;
        i |= unhex(*buf++);
    }

    return i;
}

int myatoi(const char *str, int def) {
    if (str && *str) {
        if (startsWith(str, "0x"))
            return hextoi(str);

        return atoi(str);
    }

    return def;
}

#include <stdarg.h>
#include <ctype.h>

#ifdef linux
#include <stdio.h>
#endif

static char *myutoa(char *buf, uint32_t v, uint8_t base) {
	uint32_t div = 1;

	while (v / div >= base)
		div *= base;

	while (div) {
		uint32_t n = v / div;

		v %= div;
		div /= base;

		*buf++ = n + (n >= 10 ? 'A' - 10 : '0');
	}

	*buf = 0;

	return buf;
}

static char *myitoa(char *buf, int32_t v, uint8_t base) {
	if (v < 0) {
		v *= -1;
		*buf++ = '-';
	}

	return myutoa(buf, v, 10);
}

static char *mystrcpy(char *buf, const char *src) {
	if (src)
		while (*src)
			*buf++ = *src++;

	*buf = 0;

	return buf;
}

int mysprintf(char *buf, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);

	//const char *origfmt = fmt;
	const char *start = buf;

	while (*fmt) {
		bool zero = 0;
		bool minus = 0;
		bool dword = 0;
		int len = 0;

		if (*fmt == '%') {
			char *beg = buf;
			char ch = *(++fmt);

			if (ch == '-') {
				minus = true;
				ch = *(++fmt);
			}

			while (isdigit(ch)) {
				len *= 10;
				len += ch - '0';

				if (!len)
					zero = true;

				ch = *(++fmt);
			}

			if (ch == 'l') {
				ch = *(++fmt);
				dword = true;
			}

			switch (ch) {
			case 'c':
				*buf++ = va_arg(va, int);
				break;
			case 's':
				buf = mystrcpy(buf, va_arg(va, char *));
				break;

			case 'd':
			case 'i':
				if (dword)
					buf = myitoa(buf, va_arg(va, signed long), 10);
				else
					buf = myitoa(buf, va_arg(va, signed int), 10);
				break;
			case 'u':
				if (dword)
					buf = myutoa(buf, va_arg(va, unsigned long), 10);
				else
					buf = myutoa(buf, va_arg(va, unsigned int), 10);
				break;
			case 'f':
			case 'g':
				buf = myutoa(buf, va_arg(va, double), 10);
				break;
			case 'x':
			case 'X':
				if (dword)
					buf = myutoa(buf, va_arg(va, unsigned long), 16);
				else
					buf = myutoa(buf, va_arg(va, unsigned int), 16);
				break;

			default:
				*buf++ = ch;
				break;
			}

			fmt++;

			if (len > 0) {
				int n = strlen(beg);

				if (n < len) {
					len -= n;

#ifdef linu
					printf("fmt=%s len=%d zero=%d minus=%d str=%d beg=%s\n", origfmt, len, zero, minus, n, beg);
#endif

					if (minus) {
						while (len--)
							*buf++ = ' ';
					} else {
						memmove(beg + len, beg, n + 1);

						for (uint8_t i = 0; i < len; i++)
							beg[i] = zero ? '0' : ' ';

						buf += len;

					}
				}
			}
		} else
			*buf++ = *fmt++;
	}

	*buf = 0;

	va_end(va);

	return buf - start;
}

#if 0
void _exit() {}
void _sbrk() {}
void _kill() {}
void _getpid() {}
void _write() {}
void _close() {}
void _lseek() {}
void _read() {}
void _fstat() {}
void _isatty() {}
#endif

