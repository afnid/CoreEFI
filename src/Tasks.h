// copyright to me, released under GPL V3

typedef uint32_t (*TaskCallback)(uint32_t t0, void *data);

class Task {
	const channel_t *name;
	TaskCallback cb;
	void *data;

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

	inline uint32_t call(uint32_t t0) {
		return cb(t0, data);
	}

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

	inline void init(const channel_t *name, TaskCallback cb, void *data, int32_t wait, int8_t id) {
		this->name = name;
		this->cb = cb;
		this->data = data;

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
				late = max(late, diff);

			next = now + getWait();
			loops++;

			return true;
		}

		return false;
	}

	inline void calc(int32_t ticks) {
		if (ticks > 0) {
			minticks = min(minticks, ticks);
			maxticks = max(maxticks, ticks);
		}
	}

	void send(uint32_t now) {
		channel.p1(F("tsk"));

		if (minticks != 65535)
			channel.send(F("-ticks"), TicksToMicrosf(minticks));
		else
			channel.send(F("-ticks"), TicksToMicrosf(maxticks));

		channel.send(F("+ticks"), TicksToMicrosf(maxticks));
		channel.send(F("loops"), loops);
		channel.send(F("late"), TicksToMicrosf(late));
		channel.send(F("next"), TicksToMicrosf(next));

		//float n = tdiff32(next, now) / 1000.0;
		channel.send(F("next"), TicksToMicrosf(tdiff32(next, now)));
		//channel.send(F("next"), n);

		uint32_t w = getWait();

		if (w > 1000)
			channel.send(F("kticks"), TicksToMicrosf(w / 1000.0f));
		else
			channel.send(F("ticks"), TicksToMicrosf(w));

		channel.send(name, id);
		channel.p2();
		channel.nl();

		reset();
	}
};

class TaskMgr {
public:

	static uint16_t initTasks();

	static void runTasks();

	static void addTask(const channel_t *name, TaskCallback cb, void *data, uint32_t sliceus);
};

void runInput();
