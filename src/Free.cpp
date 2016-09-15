#include "System.h"
#include "Buffer.h"
#include "Free.h"

#ifdef ARDUINO_MEGA
#include <Arduino.h>

extern uint16_t __heap_start;
extern void *__brkval;

/*
 * The free list structure as maintained by the 
 * avr-libc memory allocation routines.
 */
struct __freelist {
	size_t sz;
	struct __freelist *nx;
};

/* The head of the free list structure */
extern struct __freelist *__flp;

/* Calculates the size of the free list */
static int freeListSize() {
	struct __freelist* current;
	int total = 0;
	for (current = __flp; current; current = current->nx) {
		total += 2; /* Add two bytes for the memory block's header  */
		total += (int) current->sz;
	}
	return total;
}

uint32_t freeMemory() {
	int16_t free_memory;

	if ((int) __brkval == 0) {
		free_memory = ((int) &free_memory) - ((int) &__heap_start);
	} else {
		free_memory = ((int) &free_memory) - ((int) __brkval);
		free_memory += freeListSize();
	}

	return free_memory;
}

void sendMemory() {
}

#elif defined(ARDUINO_DUE)
extern char _end;
extern "C" char *sbrk(int i);
char *ramstart=(char *)0x20070000;
char *ramend=(char *)0x20088000;

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

uint32_t freeMemory() {
	return 0;
}

void sendMemory() {
	char *heapend = sbrk(0);
	register char * stack_ptr asm ("sp");
	struct mallinfo mi = mallinfo();
	uint32_t total = stack_ptr - heapend + mi.fordblks;

	send.p1(F("due"));
	send.json(F("dynamic_ram"), (uint32_t)(mi.uordblks), false);
	send.json(F("program_static_ram"), (uint32_t)(&_end - ramstart), false);
	send.json(F("stack_ram"), (uint32_t)(ramend - stack_ptr), false);
	send.json(F("free mem?"), total, false);
	channel.p2();
	channel.nl();
}

#else
uint32_t freeMemory() {
	return 0;
}

void sendMemory() {
}
#endif
