SRCS:=$(shell echo *.cpp)
HDRS:=$(shell echo *.h)

CFLAGS=-g -O2 -Wall -Wpedantic -Wno-variadic-macros -Wdouble-promotion -pg -DDEBUG
CFLAGS=-g -O4 -Wall -Wdouble-promotion -pg -DDEBUG
CFLAGS=-g -O4 -Wall -pg -DDEBUG

CFLAGS = -g -O2 -Wall -Wpedantic
CFLAGS += -DDEBUG
CFLAGS += -I.
CFLAGS += -fsingle-precision-constant
CFLAGS += -flto -ffunction-sections -fdata-sections -fno-builtin
CFLAGS += -fno-rtti -fno-exceptions -ffast-math
CFLAGS += -fpermissive -fno-threadsafe-statics -std=gnu++11
CFLAGS += -Wl,--gc-sections

all:
	make clean
	make efi

efi: $(SRCS) $(HDRS) Makefile
	g++ $(CFLAGS) $(SRCS) -o $@
	@size $@

clean:
	rm -f efi gmon.out

gprof:
	gprof efi

grind:
	valgrind efi

count:
	wc -l *.h
	wc -l *.cpp
