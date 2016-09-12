// copyright to me, released under GPL V3

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

	inline void send() {
		channel.p1(F("codes"));

		for (uint8_t i = 0; i < MaxParam; i++)
			if (isset(codes, i)) {
				channel.send(i, (uint16_t) 1);
				bitclr(codes, i);
			}

		channel.p2();
		channel.nl();
	}
};

EXTERN Codes codes;
