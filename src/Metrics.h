// copyright to me, released under GPL V3

void sendHist(uint16_t *hist, uint8_t count);
void sendHist(volatile uint16_t *hist, uint8_t count);

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
		minv = 0x80000000;
		maxv = -minv;
	}

	inline void add(int32_t v) {
		minv = min(minv, v);
		maxv = max(maxv, v);
	}
};

class Histogram {
	uint32_t count;
	uint32_t total;
	uint16_t counts[5];
	uint8_t max;

public:

	Histogram() {
		max = sizeof(counts) / sizeof(*counts);
		reset();
	}

	void reset() volatile {
		for (uint8_t i = 0; i < max; i++)
			counts[i] = 0;
		count = 0;
		total = 0;
	}

	inline void add(int16_t v) volatile {
		if (v) {
			if (v < 0)
				v = -v;

			if (v < max) {
				if (counts[v] + 1 != 0)
					counts[v]++;
			}

			count++;
		}

		total++;
	}

	void send(const channel_t *label, uint16_t id) volatile {
		if (total) {
			channel.p1(F("hist"));
			channel.send(F("total"), total);
			channel.send(F("count"), count);
			channel.send(label, 100.0f * count / total);

			for (uint8_t i = 0; i < max; i++) {
				if (counts[i]) {
					count -= counts[i];
					channel.send(id * i, 100.0f * count / total);
				}
			}

			sendHist(counts, max);

			channel.p2();
		}

		reset();
	}
};
