// copyright to me, released under GPL V3

class Task {
	uint16_t minticks;
	uint16_t maxticks;
	int32_t loops;
	int32_t late;

	uint32_t next;
	int32_t wait;

	uint8_t id;

	inline void reset() {
		loops = 0;
		late = 0;
		minticks = 65535;
		maxticks = 0;
	}

	uint32_t getWait() {
		return wait;
	}

public:

	inline uint8_t getId() {
		return id;
	}

	inline void setId(int8_t id) {
		if (id != -1) {
			this->next = 0;
			this->wait = 0;
			this->id = id;
		}
	}

	inline void setWait(int32_t wait) {
		assert(wait >= 0);
		this->next -= getWait();
		this->wait = max(wait, 1);
		this->next += getWait();
	}

	inline void init(int32_t wait, int8_t id) {
		setId(id);
		setWait(wait);
		reset();
	}

	inline uint32_t getNext() {
		return next;
	}

	inline void setNext(uint32_t now) {
		next = now;
	}

	inline int32_t getSleep(uint32_t now) {
		return tdiff32(now, next);
	}

	inline bool canRun(uint32_t now) {
		int32_t diff = tdiff32(now, next);

		if (diff >= 0) {
			//printf("%d %d %d\n", now / 10000, next / 10000, diff);

			if (diff)
				late = diff;

			next = now + getWait();
			loops++;

			return true;
		}

		return false;
	}

	inline void calc(int32_t ticks) {
		assert(ticks >= 0);

		if (ticks > 0) {
			minticks = min(minticks, ticks);
			maxticks = max(maxticks, ticks);
		}
	}

	void send(uint32_t now) {
		channel.p1(F("tsk"));
		channel.send(F("id"), id);

		if (minticks != 65535)
			channel.send(F("-ticks"), minticks);
		else
			channel.send(F("-ticks"), maxticks);

		channel.send(F("+ticks"), maxticks);
		channel.send(F("loops"), loops);
		channel.send(F("late"), late);
		channel.send(F("next"), next);

		//double n = tdiff32(next, now) / 1000.0;
		channel.send(F("next"), tdiff32(next, now));
		//channel.send(F("next"), n);

		uint32_t w = getWait();

		if (w > 1000)
			channel.send(F("kticks"), w / 1000.0);
		else
			channel.send(F("wait"), w);
		channel.p2();
		channel.nl();

		reset();
	}
};

uint16_t initTasks();
void runTasks();
void sendTasks();
void runInput();
