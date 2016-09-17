// copyright to me, released under GPL V3

#ifndef _Codes_h_
#define _Codes_h_

#include "Params.h"

#define PATH __FILE__

class Codes {
	uint8_t codes[bitsize(MaxParam)];

public:

	void init();

	uint16_t mem(bool alloced);

	inline void set(ParamTypeId id) {
		assert(id < MaxParam);
		bitset(codes, id);
	}

	inline void clear(ParamTypeId id) {
		assert(id < MaxParam);
		bitclr(codes, id);
	}

	inline void clear() {
		for (uint8_t i = 0; i < sizeof(codes); i++)
			codes[i] = 0;
	}

	inline bool isSet(ParamTypeId id) {
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
