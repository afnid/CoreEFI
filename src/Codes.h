// copyright to me, released under GPL V3

#ifndef _Codes_h_
#define _Codes_h_

#include "Params.h"

class Codes {
	uint8_t codes[bitsize(MaxParam)];

public:

	uint16_t init();

	inline void set(uint8_t id) {
		assert(id < MaxParam);
		bitset(codes, id);
	}

	inline void clear(uint8_t id) {
		assert(id < MaxParam);
		bitclr(codes, id);
	}

	inline void clear() {
		for (uint8_t i = 0; i < sizeof(codes); i++)
			codes[i] = 0;
	}

	inline bool isSet(uint8_t id) {
		assert(id < MaxParam);
		return isset(codes, id);
	}

	inline void send(Buffer &send) {
		send.p1(F("codes"));

		for (uint8_t i = 0; i < MaxParam; i++)
			if (isset(codes, i)) {
				send.json(i, (uint16_t) 1);
				bitclr(codes, i);
			}

		send.p2();
	}
};

EXTERN Codes codes;

#endif
