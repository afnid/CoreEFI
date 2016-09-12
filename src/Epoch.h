class Epoch {
	static uint32_t counter;
	static uint32_t last;

public:

	static void tick(uint32_t now);

	static uint16_t init();

	static uint32_t seconds();

	static uint32_t millis();

	static uint32_t micros();

	static uint32_t ticks();
};
