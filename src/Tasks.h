// copyright to me, released under GPL V3

#ifndef _Tasks_h_
#define _Tasks_h_

#include "Buffer.h"

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

	void send(Buffer &send, uint32_t now) {
		send.p1(F("tsk"));

		if (minticks != 65535)
			send.json(F("-ticks"), TicksToMicrosf(minticks));
		else
			send.json(F("-ticks"), TicksToMicrosf(maxticks));

		send.json(F("+ticks"), TicksToMicrosf(maxticks));
		send.json(F("loops"), loops);
		send.json(F("late"), TicksToMicrosf(late));
		send.json(F("next"), TicksToMicrosf(next));

		//float n = tdiff32(next, now) / 1000.0;
		send.json(F("next"), TicksToMicrosf(tdiff32(next, now)));
		//send.json(F("next"), n);

		uint32_t w = getWait();

		if (w > 1000)
			send.json(F("kticks"), TicksToMicrosf(w / 1000.0f));
		else
			send.json(F("ticks"), TicksToMicrosf(w));

		send.json(name, id);
		send.p2();

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

#endif
