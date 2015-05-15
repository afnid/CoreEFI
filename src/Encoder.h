class Encoder {
	uint32_t ratio;
	pulse_t pulse;
	uint16_t rpm;
	uint8_t edges;
	uint8_t teeth;
	uint8_t edge;
	uint8_t miss;
	bool skip;

public:

	uint16_t init() {
		ratio = 0;
		pulse = 0;
		rpm = 0;
		edges = 0;
		teeth = 0;
		edge = 0;
		miss = 0;
		return sizeof(Encoder);
	}

	void skipEncoder() {
		skip = true;
	}

	inline uint16_t refresh() {
		uint16_t t = getParamUnsigned(ConstEncoderTeeth);

		if (teeth != t + 1) {
			teeth = t + 1;
			edges = teeth << 1;
			miss = (teeth >> 1) + 1;
			rpm = 0;
			ratio = edges == 0 ? 0 : 60000000 / edges;
		}

		uint16_t rpm = getParamUnsigned(SensorDEC);

		if (this->rpm != rpm) {
			this->rpm = rpm;
			pulse = ratio == 0 || rpm == 0 ? 0 : ratio / rpm;
			pulse = MicrosToTicks(pulse);
		}

		if (pulse == 0)
			pulse = MicrosToTicks(65535);

		return pulse;
	}

	inline pulse_t run(uint32_t now) {
		if (skip)
			skip = false;
		else if (!(edge == miss || edge == miss + 1))
			decoder.run(now, edge & 1);

		if (++edge >= edges)
			edge = 0;

		return pulse;
	}
};

extern Encoder encoder;
