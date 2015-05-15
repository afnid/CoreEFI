class Codes {
	uint8_t codes[bitsize(MaxParam)];

public:

	inline uint16_t init() {
		myzero(codes, sizeof(codes));
		return sizeof(Codes);
	}

	inline void set(uint8_t id) {
		assert(id < MaxParam);
		bitset(codes, id);
	}

	inline void clear(uint8_t id) {
		assert(id < MaxParam);
		bitclr(codes, id);
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

extern Codes codes;
