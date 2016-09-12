#ifndef _Buffer_h_
#define _Buffer_h_

#include "utils.h"

class Buffer;

class Buffer {
	uint16_t beg;
	uint16_t end;
	uint16_t tbuf;
	uint8_t *buf;
	uint8_t commas;

	const uint8_t *ptr() const { return buf + beg; }

	int idx(int16_t n) const {
		if (tbuf) {
			while (n >= tbuf)
				n -= tbuf;
			while (n < 0)
				n += tbuf;
		}

		return n;
	}

	void expand(uint16_t size);

	void fmt(const char *fmt, uint32_t v);
	void out(const char *k, const char *fmt, uint32_t v);
	void out(uint16_t k, const char *fmt, uint32_t v);

public:

	Buffer();

	virtual ~Buffer() {
		clear(true);
	}

	inline bool isnl(char ch) const {
		return ch == '\n' || ch == '\r';
	}

	inline void copy(Buffer &q) {
		while (q.size() > 0)
			append(q.read());
	}

	inline int16_t last() const {
		return get(size() - 1);
	}

	inline int16_t size() const {
		return end - beg;
	}

	inline uint8_t get(int i) const {
		return i < 0 || i >= size() ? 0 : buf[idx(beg + i)];
	}

	void skip(size_t len) {
		while (size() > 0 && len-- > 0)
			read();
	}

	size_t append(char ch);

	int8_t read();

	size_t read(void *buf, size_t len);

	size_t write(const char *buf);

	size_t write(const void *buf, size_t len);

	//size_t writeln(const char *buf);

	const char *getstr();

	void trim();

	void pack();

	void clear(bool all = false);

	void dump(const char *file, int line) const;

	int tokenize(char **tokens, uint16_t max, const char *patterns);

	bool startsWith(const char *str) const;

	inline bool haseol() const {
		return size() > 0 && isnl(last());
	}

	inline bool contains(const char *s) const {
		return size() && strstr((char *)(buf + beg), s) != 0;
	}

	uint32_t crc() const {
		return crc32(buf + beg, size());
	}

	void fmt(uint32_t v) {
		fmt("%ld", (long)v);
	}

	void fmthex(uint32_t v) {
		fmt("%02lx", (long)v);
	}

#if 0
#define outf(name, type, fmt) void name(const char *k, type v) { }
#else
#define outf(name, type, fmt) void name(const char *k, type v) { out(k, fmt, v); } void name(uint16_t k, type v) { out(k, fmt, v); }
#endif

	outf(outhex, uint32_t, "%02lx");
	outf(out, bool, "%ld");
	outf(out, int8_t, "%ld");
	outf(out, uint8_t, "%lu");
	outf(out, int16_t, "%ld");
	outf(out, uint16_t, "%lu");
	outf(out, int32_t, "%ld");
	outf(out, uint32_t, "%lu");

	void out(const char *label);
	void out(const char *label, const char *v);
	void outln(const char *s = 0);
	void debug(const char *file, uint16_t line);

	void hexdump(const void *buf, uint16_t len);

	void dumptokens(const char *msg, char **tokens, uint16_t ntokens);
};

#endif
