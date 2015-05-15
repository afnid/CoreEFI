#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"

#include "Codes.h"
#include "Events.h"

#if defined(ARDUINO) && !defined(ARDUINO_UNO)
#define LOOKUPMEM
#define NAMEMEM PROGMEM
#define DATAMEM PROGMEM
#define HDRPROGMEM 0
#define DATAPROGMEM 1
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define NAMEMEM
#define LOOKUPMEM
#define DATAMEM
#define HDRPROGMEM 0
#define DATAPROGMEM 0
#endif

#include "efi_types.h"
#include "efi_data.h"

enum {
	CountRefreshes,
	CountChanges,
	CountFuncs,
	CountTables,
	CountLookups,
	HistMax,
};

static struct {
	uint8_t cached[bitsize(MaxParam)];
	uint8_t notify[bitsize(MaxParam)];
	uint16_t hist[HistMax];
	uint8_t minl;
	uint8_t maxl;
} local;

static inline void addHist(uint8_t h) {
	if (h < HistMax && local.hist[h] + 1 != 0)
		local.hist[h]++;
}

static const uint16_t multipliers[] = { 1, 4, 16, 64, 256, 1024 };

static const double divisors[] = { 1, 1.0 / (1 << 2), 1.0 / (1 << 4), 1.0 / (1 << 6), 1.0 / (1 << 8), 1.0 / (1 << 10), };

#if HDRPROGMEM | DATAPROGMEM
static void mymemcpy_PF(char *dst, char *src, int bytes)
{
	//extern void *memcpy_PF (void *dest, uint_farptr_t src, size_t len);

	for (uint8_t id = 0; id < bytes; id++)
		*dst++ = pgm_read_byte_near(src++);
}
#endif

static inline LookupHeader *getLookupHeader(const ParamData *pd) {
	assert(pd->hdr);

#if HDRPROGMEM
	static LookupHeader h;

	if (pd && pd->hdr)
		mymemcpy_PF((char *)h, (char *)(lookups + pd->hdr - 1), sizeof(LookupHeader));
	else
		myzero(h, sizeof(LookupHeader));
#else
	return lookups + pd->hdr - 1;
#endif
}

static inline const channel_t *getName(uint8_t id) {
#ifdef ARDUINO
	return (channel_t *)(names + pgm_read_word_near(nameidx + id));
#else
	return names + nameidx[id];
#endif
}

/*
 static void getLookupData(const LookupHeader *h, uint16_t *buf, int len)
 {
 if (h) {
 #if DATAPROGMEM
 xx
 mymemcpy_PF((char *)buf, (char *)(data + h->offset), sizeof(*data) * min(len, h->length));
 #else
 memcpy(buf, data + h->offset, sizeof(*data) * min(len, h->length));
 #endif
 } else
 myzero(buf, sizeof(*data) * len);
 }
 */

static inline uint16_t getLookupCell(const LookupHeader *h, int x) {
#if DATAPROGMEM
	return pgm_read_word_near(data + h->offset + x);
#else
	return data[h->offset + x];
#endif
}

static inline ParamData *getParamData(uint8_t id) {
	assert(id >= 0 && id < MaxParam);
	return params + id;
}

static inline uint16_t _uint16Decode(ParamData *pd, uint16_t v) {
	assert(!pd->div);
	assert(!pd->neg);
	return v;
}

static inline int16_t int16Decode(ParamData *pd, uint16_t v) {
	assert(!pd->div);
	assert(pd->neg);
	return v;
}

static inline double _dblDecode(ParamData *pd, uint16_t v) {
	if (v == 0)
		return 0;

	if (!pd->div) {
		if (pd->neg)
			return int16Decode(pd, v);
		return _uint16Decode(pd, v);
	}

	double x = pd->neg ? (int) v : v;
	return x * divisors[pd->div];
}

static inline uint16_t uint16Encode(uint8_t id, ParamData *pd, int v) {
	if (v) {
		if (v < _dblDecode(pd, pd->min)) {
			codes.set(id);
			//return pd->min;
		}

		if (v > _dblDecode(pd, pd->max)) {
			codes.set(id);
			//return pd->max;
		}
	}

	assert(!pd->div);
	return v;
}

static inline uint16_t dblEncode(uint8_t id, ParamData *pd, double v) {
	if (v) {
		if (v < _dblDecode(pd, pd->min)) {
			codes.set(id);
			//return pd->min;
		}

		if (v > _dblDecode(pd, pd->max)) {
			codes.set(id);
			//return pd->max;
		}

		if (pd->div)
			v *= multipliers[pd->div];
	}

	return (uint16_t) v;
}

static void clearCached(uint8_t id, ParamData *pd) {
	if (pd->cat == CatConst) {
		myzero(local.cached, sizeof(local.cached));
	} else {
		for (uint8_t i = local.minl; i <= local.maxl; i++) {
			if (isset(local.cached, i)) {
				ParamData *pd2 = getParamData(i);

				if (pd2->hdr) {
					LookupHeader *h = getLookupHeader(pd2);

					if (h->cid - 1 == id || h->rid - 1 == id)
						bitclr(local.cached, i); //expireParamCache(i); no reason to recurse, no func/table is dependent on another func/table
				}
			}
		}

		extern void expireCached(uint8_t id);
		expireCached(id);
	}
}

void clearCached(uint8_t id) {
	ParamData *pd = getParamData(id);
	bitclr(local.cached, id);
	clearCached(id, pd);
}

static inline void setEncoded(uint8_t id, ParamData *pd, uint16_t v) {
	if (pd->value != v) {
		addHist(CountChanges);
		clearCached(id, pd);
		bitset(local.notify, id);
		pd->value = v;
	}
}

static inline void refreshValue(uint8_t id, ParamData *pd) {
	if (!pd->hdr || !isset(local.cached, id)) {
		extern double getStrategyDouble(uint8_t id, ParamData *pd);
		double v = getStrategyDouble(id, pd);
		uint16_t i = dblEncode(id, pd, v);

		setEncoded(id, pd, i);
		addHist(CountRefreshes);
		bitset(local.cached, id);
	}
}

static inline double getVal(ParamData *pd, LookupHeader *h, uint8_t x, uint8_t y) {
	uint16_t i = y * h->cols + x;
	uint16_t raw = getLookupCell(h, i);
	return _dblDecode(pd, raw);
}

static inline int floor(double v) {
	int x = (int) v;
	return x < v ? x : x + 1;
}

static inline bool equivelent(double d1, double d2) {
	static const double close = 0.001;
	return abs(d1 - d2) <= close;
}

void setParamShort(uint8_t id, int16_t v) {
	ParamData *pd = getParamData(id);
	setEncoded(id, pd, uint16Encode(id, pd, v));
}

void setParamUnsigned(uint8_t id, uint16_t v) {
	ParamData *pd = getParamData(id);
	setEncoded(id, pd, uint16Encode(id, pd, v));
}

void setParamDouble(uint8_t id, double v) {
	ParamData *pd = getParamData(id);
	setEncoded(id, pd, dblEncode(id, pd, v));
}

bool isParamSet(uint8_t id) {
	ParamData *pd = getParamData(id);
	return pd->value;
}

int16_t getParamShort(uint8_t id) {
	ParamData *pd = getParamData(id);
	refreshValue(id, pd);
	return !pd ? 0 : int16Decode(pd, pd->value);
}

uint16_t getParamUnsigned(uint8_t id) {
	ParamData *pd = getParamData(id);
	refreshValue(id, pd);
	return !pd ? 0 : _uint16Decode(pd, pd->value);
}

double getParamDouble(uint8_t id) {
	ParamData *pd = getParamData(id);
	refreshValue(id, pd);
	return !pd ? 0 : _dblDecode(pd, pd->value);
}

uint16_t uint16Decode(ParamData *pd, uint16_t v) {
	return _uint16Decode(pd, v);
}

double dblDecode(ParamData *pd, uint16_t v) {
	return _dblDecode(pd, v);
}

void clearParamCache() {
	for (uint8_t id = 0; id < MaxParam; id++) {
		//ParamData *pd = getParamData(id);

		//if (pd->cat == CatFlag || pd->cat == CatCalc || pd->cat == CatTime)
			bitclr(local.cached, id);
	}
}

void clearParamChanges() {
	//myzero(local.cached, sizeof(local.cached));
}

void setSensorParam(uint8_t id, uint16_t adc) {
	ParamData *pd = getParamData(id);
	clearCached(id, pd); // delete
	//setEncoded(id, pd, dblEncode(id, pd, adc));
}

uint16_t initParams() {
	myzero(&local, sizeof(local));
	local.minl = MaxParam;

	for (uint8_t id = 0; id < MaxParam; id++) {
		ParamData *pd = getParamData(id);

		if (pd->hdr) {
			local.minl = min(local.minl, id);
			local.maxl = max(local.maxl, id);
		}
	}

	return sizeof(local) + sizeof(params) + sizeof(lookups);
}

static void sendParam(uint8_t id, ParamData *pd) {
	channel.p1(F("u"));

	if (false && isParamSet(FlagIsMonitoring)) {
		if (pd->div)
			channel.send(id, _dblDecode(pd, pd->value));
		else if (pd->neg)
			channel.send(id, int16Decode(pd, pd->value));
		else
			channel.send(id, _uint16Decode(pd, pd->value));
	} else {
		channel.send(F("cat"), pd->cat);

		if (pd->div)
			channel.send(id, _dblDecode(pd, pd->value));
		else if (pd->neg)
			channel.send(id, int16Decode(pd, pd->value));
		else
			channel.send(id, _uint16Decode(pd, pd->value));

		if (pd->div)
			channel.send(getName(id), _dblDecode(pd, pd->value));
		else if (pd->neg)
			channel.send(getName(id), int16Decode(pd, pd->value));
		else
			channel.send(getName(id), _uint16Decode(pd, pd->value));
	}

	channel.p2();
	channel.nl();
}

void sendParamValues() {
	for (uint8_t id = 0; id < MaxParam; id++) {
		ParamData *pd = getParamData(id);

		if (getParamDouble(id))
			sendParam(id, pd);
	}
}

void sendParamChanges() {
	for (uint8_t id = 0; id < MaxParam; id++) {
		if (isset(local.notify, id)) {
			ParamData *pd = getParamData(id);
			sendParam(id, pd);
			bitclr(local.notify, id);
		}
	}
}

void sendParamLookups() {
	for (uint8_t id = 0; id < MaxParam; id++) {
		ParamData *pd = getParamData(id);

		if (pd->hdr) {
			LookupHeader *h = getLookupHeader(pd);

			channel.p1(F("l"));
			channel.send(F("hdr"), pd->hdr);
			channel.send(F("cid"), h->cid);
			channel.send(F("rid"), h->rid);
			channel.send(F("r1"), h->r1);
			channel.send(F("r2"), h->r2);
			channel.send(F("c1"), h->c1);
			channel.send(F("c2"), h->c2);
			channel.send(F("rows"), h->rows);
			channel.send(F("cols"), h->cols);
			channel.send(F("len"), h->length);
			channel.send(F("off"), h->offset);
			channel.send(F("interp"), h->interp);
			channel.send(F("last"), h->last);
			channel.send(F("cached"), isset(local.cached, id));
			channel.p2();
			channel.nl();
		}
	}
}

void sendParamList() {
	for (uint8_t id = 0; id < MaxParam; id++) {
		ParamData *pd = getParamData(id);

		channel.p1(F("p"));
		channel.send(F("id"), id);
		channel.send(F("cat"), pd->cat);
		channel.send(F("div"), pd->div);

		channel.send(F("min"), pd->min);
		channel.send(F("max"), pd->max);
		channel.send(F("mind"), _dblDecode(pd, pd->min));
		channel.send(F("maxd"), _dblDecode(pd, pd->max));
		channel.p2();
		channel.nl();
	}
}

void sendParamStats() {
	channel.p1(F("params"));
	channel.send(F("params"), (uint16_t) sizeof(params));
	channel.send(F("lookups"), (uint16_t) sizeof(lookups));
	channel.send(F("names"), (uint16_t) sizeof(names));
	channel.send(F("nameidx"), (uint16_t) sizeof(nameidx));
	channel.send(F("data"), (uint16_t) sizeof(data));
	channel.send(F("total"), (uint16_t)(sizeof(params) + sizeof(lookups) + sizeof(names) + sizeof(nameidx) + sizeof(data)));
	channel.send(F("ParamData"), (uint16_t) sizeof(ParamData));
	channel.p2();
	sendHist(local.hist, HistMax);
	//Metrics::send(local.metrics, MaxParam);
	channel.nl();
}

double lookupParam(uint8_t id, ParamData *pd) {
	LookupHeader *h = getLookupHeader(pd);

	if (h->cid && h->rid) {
		ParamData *cp = getParamData(h->cid - 1);
		ParamData *rp = getParamData(h->rid - 1);
		addHist(CountTables);

		double x = getParamDouble(h->cid - 1);
		double y = getParamDouble(h->rid - 1);
		double c1 = _dblDecode(cp, h->c1);
		double c2 = _dblDecode(cp, h->c2);
		double r1 = _dblDecode(rp, h->r1);
		double r2 = _dblDecode(rp, h->r2);

		double yy = (y - r1) * (h->rows - 1) / (r2 - r1);
		double xx = (x - c1) * (h->cols - 1) / (c2 - c1);

		yy = max(0, min(yy, h->rows - 1));
		xx = max(0, min(xx, h->cols - 1));

		int ix = (int) xx;
		int iy = (int) yy;

		if (ix <= 0)
			return getVal(pd, h, 0, iy);
		if (ix >= h->cols - 1)
			return getVal(pd, h, h->cols - 1, iy);

		ix = floor(xx);

		double v0 = getVal(pd, h, ix, iy);
		double v1 = getVal(pd, h, ix + 1, iy);
		return v0 + (xx - ix) * (v1 - v0);
	}

	if (h->cid && h->rows == 2) {
		addHist(CountFuncs);

		ParamData *cp = getParamData(h->cid - 1);
		uint8_t r = h->rows - 1;

		double y = getParamDouble(h->cid - 1);
		double y1 = getVal(cp, h, h->last, 0);

		while (h->last > 0 && y < y1)
			y1 = getVal(cp, h, --(h->last), 0);

		double v = 0;

		while (h->last < h->cols - 1 && y >= (v = getVal(cp, h, h->last + 1, 1))) {
			h->last++;
			y1 = v;
		}

		if (h->last >= h->cols - 1 && y >= y1)
			return getVal(pd, h, h->last, r);
		if (h->last <= 0 && y <= y1)
			return getVal(pd, h, h->last, r);
		if (equivelent(y, y1))
			return getVal(pd, h, h->last, r);

		double y2 = getVal(cp, h, h->last + 1, 0);

		if (y1 == y2 || equivelent(y, y2))
			return getVal(pd, h, h->last, r);

		double pct = (y - y1) / (y2 - y1);
		double v1 = getVal(pd, h, h->last - 1, r);
		double v2 = getVal(pd, h, h->last, r);
		return v1 + pct * (v2 - v1);
	}

	if (h->rows == 1) {
		// linear function, value provides index, can be in-place for adc

		addHist(CountLookups);

		double y = h->cid ? getParamDouble(h->cid - 1) : _dblDecode(pd, pd->value);

		if (y <= h->c1 || h->c1 == h->c2)
			return getVal(pd, h, 0, 0);
		if (y >= h->c2)
			return getVal(pd, h, h->cols - 1, 0);

		double px = (y - h->c1) / (h->c2 - h->c1);
		h->last = floor(px);

		if (!h->interp || h->last == h->cols - 1 || equivelent(h->last, px))
			return getVal(pd, h, h->last, 0);

		double pct = y - h->last; // 0-1
		double v1 = getVal(pd, h, h->last, 0);
		double v2 = getVal(pd, h, h->last + 1, 0);
		return v1 + pct * (v2 - v1);
	}

	//assert(0);

	return 0;
}
