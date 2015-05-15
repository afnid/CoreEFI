// copyright to me, released under GPL V3

class Metric {
	uint32_t ticks;

public:

	void init();
	void calc();

	static void send(const Metric *metrics, uint8_t count, uint16_t over = 0);
};

class MinMax {
public:

	int32_t minv;
	int32_t maxv;

	inline void init() {
		minv = 1L << (sizeof(minv) * 8 - 1);
		maxv = -minv;
	}

	inline void add(int32_t v) {
		minv = min(minv, v);
		maxv = max(maxv, v);
	}
};

void sendHist(uint16_t *hist, uint8_t count);
void sendHist(volatile uint16_t *hist, uint8_t count);
