// copyright to me, released under GPL V3

#ifndef _Decoder_h_
#define _Decoder_h_

#define PATH __FILE__

#include "System.h"
#include "Params.h"

class Decoder {
	enum {
		CountCycles,
		CountTotals,
		CountNeg,

		CountSync,
		CountTriggers,
		CountComputes,

		CountSkips,
		CountReKeys,
		CountRevChange,

		CountRefresh,
		CountOverMax,
		CountTooFast,

		HistMax,
	};

	uint32_t last;
	uint32_t sum;

	uint32_t tdcts;
	uint32_t total;

	pulse_t pulses[MaxEncoderTeeth * 2];
	uint16_t hist[HistMax];

	uint8_t edges;
	uint8_t valid;

	int8_t key;
	uint8_t tdc;
	uint8_t old;

	int8_t idx;

	// parameter cache for speed/volatility
	uint8_t teeth;
	uint8_t compare;
	uint8_t offset;
	bool synchigh;
	bool syncshort;

	static inline bool ishi(uint8_t i) {
		return (i & 1) != 0;
	}

	inline void addHist(uint8_t h) volatile {
		if (h < HistMax && hist[h] + 1 != 0)
			hist[h]++;
	}

	inline int index(int i) volatile {
		if (!edges)
			return 0;
		if (i >= edges)
			i -= edges;
		if (i < 0)
			i += edges;
		return i;
	}

	inline void findSync() volatile {
		if (valid) {
			uint32_t t = 0;
			int8_t mini = -1;
			int8_t maxi = -1;

			for (uint8_t i = 0; i < valid; i++) {
				t += pulses[i];

				if (mini < 0 || pulses[i] < pulses[mini])
					mini = i;

				if (maxi < 0 || pulses[i] > pulses[maxi])
					maxi = i;
			}

			if (sum != t) {
				sum = t;
				addHist(CountRevChange);
			}

			uint8_t i = index(syncshort ? mini : maxi);

			if (key != i) {
				key = i;
				old = tdc;
				tdc = index(key + offset);
				addHist(CountReKeys);
			}

			addHist(CountComputes);
		}
	}

	inline bool isSynced() volatile {
		if (key != idx && valid >= 4 && ishi(idx) == synchigh) {
			uint8_t i0 = index(idx);
			uint8_t i1 = index(idx - compare);
			pulse_t p0 = pulses[i0];
			pulse_t p1 = pulses[i1];
			pulse_t p02 = p0 >> 1;
			pulse_t p12 = p1 >> 1;

			if (p0 + p02 > p1 || p1 + p12 > p0) { // Min 50% difference
				if (!syncshort && p02 > p1) {
					addHist(CountTriggers);
					return false;
				}

				if (syncshort && p12 > p0) {
					addHist(CountTriggers);
					return false;
				}
			}
		}

		return true;
	}

public:

	inline void run(uint32_t now, bool hi) volatile {
		int32_t pulse = tdiff32(now, last);

		if (pulse < 32)
			addHist(CountTooFast);
		else if ((uint32_t)pulse != (pulse_t)pulse)
			addHist(CountOverMax);
		else if (!edges)
			addHist(CountSkips);
		else if (last) {
			if (hi == ishi(idx)) {
				idx = index(idx + 1);
				key = min(-1, -key);
				addHist(CountSync);
			}

			pulse_t old = pulses[idx];
			pulses[idx] = pulse;
			sum += pulse;
			total += pulse;

			assert(pulses[idx] == (uint32_t)pulse);

			if (old > total) { // can go neg on restart pulses?
				total -= pulse;
				addHist(CountNeg);
			} else
				total -= old;

			if (valid < edges)
				valid++;

			if (key < 0 || !isSynced())
				findSync();

			if (idx == tdc) {
				if (sum != total) {
					total = sum;
					addHist(CountTotals);
				}

				tdcts = now;
				sum = 0;

				addHist(CountCycles);

				extern void setEventTDC(uint32_t tdcts);
				setEventTDC(tdcts);
			}

			idx = index(idx + 1);
		}

		last = now;
	}

	inline bool isValid() volatile {
		return valid >= edges;
	}

	inline uint32_t getTDC() volatile {
		return tdcts;
	}

	inline uint32_t getTicks() volatile {
		return total;
	}

	inline uint16_t getRPM() volatile {
		return total == 0 ? 0 : 60000000UL / TicksToMicros(total);
	}

	inline void refresh(uint32_t now) volatile {
		uint8_t teeth = getParamUnsigned(ConstEncoderTeeth);

		if (this->teeth != teeth) {
			this->teeth = teeth;
			edges = teeth * 2;
			assert(edges <= MaxEncoderTeeth * 2);
			assert(edges > 0);
		}

		compare = getParamUnsigned(ConstEncoderCompare);
		offset = getParamUnsigned(ConstEncoderCountOffset);
		synchigh = isParamSet(ConstEncoderSyncHigh);
		syncshort = isParamSet(ConstEncoderSyncShort);

		if (tdiff32(now, last) > (int32_t)MicrosToTicks(65535u)) {
			addHist(CountRefresh);
			total = 0;
			valid = 0;
		}
	}

	void init() volatile;

	uint16_t mem(bool alloced) volatile;

	void sendStatus(Buffer &send) volatile;

	void sendList(Buffer &send) volatile;
};

#undef PATH

EXTERN volatile Decoder decoder;

#endif
