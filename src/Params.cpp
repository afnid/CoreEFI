// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Metrics.h"
#include "Params.h"
#include "Prompt.h"
#include "Tasks.h"
#include "Codes.h"

#if defined(ARDUINO_UNO)
#define LOOKUPPROGMEM 1
#define DATAPROGMEM 1
#define NAMEPROGMEM 1
#elif defined(ARDUINO)
#define LOOKUPPROGMEM 0
#define DATAPROGMEM 0
#define NAMEPROGMEM 1
#else
#define LOOKUPPROGMEM 0
#define DATAPROGMEM 0
#define NAMEPROGMEM 0
#endif

#if LOOKUPPROGMEM
#define LOOKUPMEM PROGMEM
#include <avr/pgmspace.h>
#else
#define LOOKUPMEM
#endif

#if NAMEPROGMEM
#define NAMEMEM PROGMEM
#include <avr/pgmspace.h>
#else
#define NAMEMEM
#endif

#if DATAPROGMEM
#define DATAMEM PROGMEM
#include <avr/pgmspace.h>
#else
#define DATAMEM
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

static const float divisors[] = { 1, 1.0 / (1 << 2), 1.0 / (1 << 4), 1.0 / (1 << 6), 1.0 / (1 << 8), 1.0 / (1 << 10), };

#if LOOKUPPROGMEM | DATAPROGMEM
static void mymemcpy_PF(char *dst, char *src, int bytes)
{
	//extern void *memcpy_PF (void *dest, uint_farptr_t src, size_t len);

	for (uint8_t id = 0; id < bytes; id++)
		*dst++ = pgm_read_byte_near(src++);
}
#endif

static inline LookupHeader *getLookupHeader(const ParamData *pd) {
	assert(pd->hdr);

#if LOOKUPPROGMEM
	static LookupHeader h;

	if (pd && pd->hdr)
		mymemcpy_PF((char *)h, (char *)(lookups + pd->hdr - 1), sizeof(LookupHeader));
	else
		myzero(h, sizeof(LookupHeader));
#else
	return lookups + pd->hdr - 1;
#endif
}

static inline const channel_t *getName(ParamTypeId id) {
#if NAMEPROGMEM
	return (channel_t *)(names + pgm_read_word_near(nameidx + id));
#else
	return names + nameidx[id];
#endif
}

const channel_t *getParamName(ParamTypeId id) {
	return getName(id);
}

const char *getParamName(ParamTypeId id, char *buf, int maxlen) {
#if NAMEPROGMEM
	const uint8_t *src = (uint8_t *)getName(id);
	char *s = buf;
	char ch = 0;
	int n = 0;

	while (++n < maxlen && (ch = pgm_read_byte_near(src++)))
		*s++ = ch;

	*s = 0;
#else
	strncpy(buf, getName(id), maxlen);
	buf[maxlen - 1] = 0;
#endif

	return buf;
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

static inline float _dblDecode(ParamData *pd, uint16_t v) {
	if (v == 0)
		return 0;

	if (!pd->div) {
		if (pd->neg)
			return int16Decode(pd, v);
		return _uint16Decode(pd, v);
	}

	float x = pd->neg ? (int) v : v;
	return x * divisors[pd->div];
}

static inline uint16_t uint16Encode(ParamTypeId id, ParamData *pd, int v) {
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

static inline uint16_t dblEncode(ParamTypeId id, ParamData *pd, float v) {
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

static void clearCached(ParamTypeId id, ParamData *pd) {
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

		extern void expireCached(ParamTypeId id);
		expireCached(id);
	}
}

void clearCached(ParamTypeId id) {
	ParamData *pd = getParamData(id);
	bitclr(local.cached, id);
	clearCached(id, pd);
}

static inline void setEncoded(ParamTypeId id, ParamData *pd, uint16_t v) {
	if (pd->value != v) {
		addHist(CountChanges);
		clearCached(id, pd);
		bitset(local.notify, id);
		pd->value = v;
	}
}

static inline void refreshValue(ParamTypeId id, ParamData *pd) {
	if (!pd->hdr || !isset(local.cached, id)) {
		extern float getStrategyDouble(ParamTypeId id, ParamData *pd);
		float v = getStrategyDouble(id, pd);
		uint16_t i = dblEncode(id, pd, v);

		setEncoded(id, pd, i);
		addHist(CountRefreshes);
		bitset(local.cached, id);
	}
}

static inline float getVal(ParamData *pd, LookupHeader *h, uint8_t x, uint8_t y) {
	uint16_t i = y * h->cols + x;
	uint16_t raw = getLookupCell(h, i);
	return _dblDecode(pd, raw);
}

static inline int floor(float v) {
	int x = (int) v;
	return x < v ? x : x + 1;
}

static inline bool equivelent(float d1, float d2) {
	static const float close = 0.001;
	return abs(d1 - d2) <= close;
}

void setParamShort(ParamTypeId id, int16_t v) {
	ParamData *pd = getParamData(id);
	setEncoded(id, pd, uint16Encode(id, pd, v));
}

void setParamUnsigned(ParamTypeId id, uint16_t v) {
	ParamData *pd = getParamData(id);
	setEncoded(id, pd, uint16Encode(id, pd, v));
}

void setParamFloat(ParamTypeId id, float v) {
	ParamData *pd = getParamData(id);
	setEncoded(id, pd, dblEncode(id, pd, v));
}

bool isParamSet(ParamTypeId id) {
	ParamData *pd = getParamData(id);
	return pd->value;
}

int16_t getParamShort(ParamTypeId id) {
	ParamData *pd = getParamData(id);
	refreshValue(id, pd);
	return !pd ? 0 : int16Decode(pd, pd->value);
}

uint16_t getParamUnsigned(ParamTypeId id) {
	ParamData *pd = getParamData(id);
	refreshValue(id, pd);
	return !pd ? 0 : _uint16Decode(pd, pd->value);
}

float getParamFloat(ParamTypeId id) {
	ParamData *pd = getParamData(id);
	refreshValue(id, pd);
	return !pd ? 0 : _dblDecode(pd, pd->value);
}

uint16_t uint16Decode(ParamData *pd, uint16_t v) {
	return _uint16Decode(pd, v);
}

float dblDecode(ParamData *pd, uint16_t v) {
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

void setSensorParam(ParamTypeId id, uint16_t adc) {
	ParamData *pd = getParamData(id);
	clearCached(id, pd); // delete
	//setEncoded(id, pd, dblEncode(id, pd, adc));
}

static void sendParam(ParamTypeId id, ParamData *pd, bool nl) {
	if (nl)
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

	if (nl) {
		channel.p2();
		channel.nl();
	}
}

void sendParam(ParamTypeId id) {
	ParamData *pd = getParamData(id);
	sendParam(id, pd, false);
}

static void sendParamChanges(void *) {
	for (uint8_t id = 0; id < MaxParam; id++) {
		if (isset(local.notify, id)) {
			ParamData *pd = getParamData(id);
			sendParam((ParamTypeId)id, pd, true);
			bitclr(local.notify, id);
		}
	}
}

static void sendParamValues(void *) {
	for (uint8_t id = 0; id < MaxParam; id++) {
		ParamData *pd = getParamData(id);
		sendParam((ParamTypeId)id, pd, true);
	}
}

static void sendParamLookups(void *) {
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

static void sendParamList(void *) {
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

static void sendParamStats(void *) {
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

static inline uint32_t runChanges(uint32_t t0, void *data) {
	uint32_t wait = getParamUnsigned(FlagIsMonitoring);

	if (wait) {
		sendParamChanges(0);
		wait = max(wait, 500);
		wait = min(wait, 1000);
		return wait;
	}

	return 3000017UL;
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

	TaskMgr::addTask(F("Params"), runChanges, 0, 3000);

	static PromptCallback callbacks[] = {
		{ F("pv"), sendParamValues, 0, F("values") },
		{ F("pt"), sendParamLookups, 0, F("tables") },
		{ F("pl"), sendParamList, 0, F("list") },
		{ F("ps"), sendParamStats, 0, F("stats") },
		{ F("pc"), sendParamChanges, 0, F("changes") },
	};

	addPromptCallbacks(callbacks, ARRSIZE(callbacks));

	return sizeof(local) + sizeof(params) + sizeof(lookups) + sizeof(callbacks);
}

float lookupParam(ParamTypeId id, ParamData *pd) {
	LookupHeader *h = getLookupHeader(pd);

	if (h->cid && h->rid) {
		ParamData *cp = getParamData(h->cid - 1);
		ParamData *rp = getParamData(h->rid - 1);
		addHist(CountTables);

		float x = getParamFloat((ParamTypeId)(h->cid - 1));
		float y = getParamFloat((ParamTypeId)(h->rid - 1));
		float c1 = _dblDecode(cp, h->c1);
		float c2 = _dblDecode(cp, h->c2);
		float r1 = _dblDecode(rp, h->r1);
		float r2 = _dblDecode(rp, h->r2);

		float yy = (y - r1) * (h->rows - 1) / (r2 - r1);
		float xx = (x - c1) * (h->cols - 1) / (c2 - c1);

		yy = max(0, min(yy, h->rows - 1));
		xx = max(0, min(xx, h->cols - 1));

		int ix = (int) xx;
		int iy = (int) yy;

		if (ix <= 0)
			return getVal(pd, h, 0, iy);
		if (ix >= h->cols - 1)
			return getVal(pd, h, h->cols - 1, iy);

		ix = floor(xx);

		float v0 = getVal(pd, h, ix, iy);
		float v1 = getVal(pd, h, ix + 1, iy);
		return v0 + (xx - ix) * (v1 - v0);
	}

	if (h->cid && h->rows == 2) {
		addHist(CountFuncs);

		ParamData *cp = getParamData(h->cid - 1);
		uint8_t r = h->rows - 1;

		float y = getParamFloat((ParamTypeId)(h->cid - 1));
		float y1 = getVal(cp, h, h->last, 0);

		while (h->last > 0 && y < y1)
			y1 = getVal(cp, h, --(h->last), 0);

		float v = 0;

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

		float y2 = getVal(cp, h, h->last + 1, 0);

		if (y1 == y2 || equivelent(y, y2))
			return getVal(pd, h, h->last, r);

		float pct = (y - y1) / (y2 - y1);
		float v1 = getVal(pd, h, h->last - 1, r);
		float v2 = getVal(pd, h, h->last, r);
		return v1 + pct * (v2 - v1);
	}

	if (h->rows == 1) {
		// linear function, value provides index, can be in-place for adc

		addHist(CountLookups);

		float y = h->cid ? getParamFloat((ParamTypeId)(h->cid - 1)) : _dblDecode(pd, pd->value);

		if (y <= h->c1 || h->c1 == h->c2)
			return getVal(pd, h, 0, 0);
		if (y >= h->c2)
			return getVal(pd, h, h->cols - 1, 0);

		float px = (y - h->c1) / (h->c2 - h->c1);
		h->last = floor(px);

		if (!h->interp || h->last == h->cols - 1 || equivelent(h->last, px))
			return getVal(pd, h, h->last, 0);

		float pct = y - h->last; // 0-1
		float v1 = getVal(pd, h, h->last, 0);
		float v2 = getVal(pd, h, h->last + 1, 0);
		return v1 + pct * (v2 - v1);
	}

	//assert(0);

	return 0;
}
