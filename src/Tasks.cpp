// copyright to me, released under GPL V3

#include "System.h"
#include "Channel.h"
#include "Prompt.h"
#include "Metrics.h"
#include "Params.h"

#include "Tasks.h"
#include "Free.h"

static const uint8_t TaskMax = 15;

typedef struct _Node {
	Task task;
	struct _Node *next;
	struct _Node *prev;

	inline void verify() {
		assert(next);
		assert(prev);
		assert(next->prev == this);
		assert(prev->next == this);
	}

	inline void unlink() {
		next->prev = prev;
		prev->next = next;
	}

	inline void append(struct _Node *n) {
		n->next = next;
		n->prev = this;
		next->prev = n;
		next = n;
		verify();
	}

	inline void insert(struct _Node *n) {
		n->next = this;
		n->prev = prev;
		prev->next = n;
		prev = n;
		verify();
	}

	void send(struct _Node *zero) {
		assert(this - zero >= 0);
		assert(this - zero < TaskMax);
		channel.p1(F("task"));
		channel.send(F("i"), (uint16_t) (this - zero));
		channel.send(F("n"), (uint16_t) (next - zero));
		channel.send(F("p"), (uint16_t) (prev - zero));
		channel.p2();
		verify();
	}
} Node;

static struct {
	Node queue[TaskMax];
	Node *head;
	uint32_t start;
	uint32_t slept;
	uint32_t busy;
	uint8_t ntasks;

	inline void verify(Node *h) {
		assert(h == head || tdiff32(h->task.getNext(), h->prev->task.getNext()) >= 0);
		assert(h->next == head || tdiff32(h->task.getNext(), h->next->task.getNext()) <= 0);
		h->verify();
	}

	inline void scheduleTask(Node *n) {
		Node *h = head;

		while (tdiff32(n->task.getNext(), h->task.getNext()) >= 0 && h->next != head)
			h = h->next;

		if (tdiff32(h->task.getNext(), n->task.getNext()) > 0) {
			h->insert(n);

			if (h == head)
				head = n;
		} else
			h->append(n);

		verify(n);
	}

	inline void reschedule(Node *n) {
		if (n == head)
			head = n->next;
		n->unlink();
		scheduleTask(n);
	}

	inline void setEpoch() {
		uint32_t now = clockTicks();

		for (uint8_t i = 0; i < TaskMax; i++)
			queue[i].task.setNext(now);

		start = now;
	}

	inline void addTask(const channel_t *name, TaskCallback cb, void *data, uint32_t waitms) {
		assert(ntasks < ARRSIZE(queue));

		if (ntasks < ARRSIZE(queue)) {
			queue[ntasks].next = queue + ntasks;
			queue[ntasks].prev = queue + ntasks;
			queue[ntasks].task.init(name, cb, data, MicrosToTicks(waitms * 1000), ntasks);
			ntasks++;
		}
	}

	void boot() {
		head = queue;

		for (uint8_t i = 1; i < ntasks; i++)
			scheduleTask(queue + i);

		setEpoch();
	}
} tasks;

static inline void runTest() {
	for (uint16_t i = 0; i < 100; i++)
		getParamUnsigned(ConstEncoderTeeth);

	//setParamFloat(SensorVSS, getParamUnsigned(CalcRPMtoMPH));
	//getParamFloat(CalcFinalLamda);
	//getParamFloat(CalcMPG);
	//getParamFloat(CalcDutyCycle);
}

static void sendTasks(void *data) {
	uint32_t now = clockTicks();
	Node *n = tasks.head;

	uint32_t since = tdiff32(now, tasks.slept);
	float duty = !since ? 0 : 100.0f * tasks.busy / since;

	channel.p1(F("tasks"));
	channel.send(F("duty"), duty);
	channel.send(F("ntasks"), tasks.ntasks);
	channel.p2();
	channel.nl();

	tasks.slept = now;
	tasks.busy = 0;

	do {
		n->task.send(now);
		n = n->next;
	} while (n != tasks.head);
}

void TaskMgr::addTask(const channel_t *name, TaskCallback cb, void *data, uint32_t waitms) {
	tasks.addTask(name, cb, data, waitms);
}

static int32_t runTask(uint32_t now) {
	if (tasks.head->task.canRun(now)) {
		Node *n = tasks.head;

		tasks.reschedule(n); // TODO Twice?

		uint32_t t0 = clockTicks();
		uint32_t ticks = n->task.call(t0);

		if (ticks != 0) {
			n->task.setWait(ticks);
			tasks.reschedule(n);
		}

		now = clockTicks();
		uint32_t busy = tdiff32(now, t0);
		tasks.busy += busy;
		n->task.calc(busy);
	}

	return tasks.head->task.getSleep(now);
}

void TaskMgr::runTasks() {
	tasks.boot();

	while (true)
		runTask(clockTicks());
}

void runPerfectTasks() {
	tasks.boot();

	uint32_t last = clockTicks();

	while (true) {
		uint32_t now = clockTicks();

		if (tdiff32(now, last) > 0) {
			int32_t sleep = runTask(last);

			if (false && sleep > 0) {
				last += sleep;
			} else
				last += 2; // perfect resolution
		}
	}
}

uint16_t TaskMgr::initTasks() {
	bzero(&tasks, sizeof(tasks));

	static PromptCallback callbacks[] = {
		{ F("t"), sendTasks, 0, F("tasks") },
	};

	addPromptCallbacks(callbacks, ARRSIZE(callbacks));

	return sizeof(tasks) + sizeof(callbacks);
}
