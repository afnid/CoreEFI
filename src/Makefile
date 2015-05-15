SRCS=\
 Channel.cpp\
 Events.cpp\
 Free.cpp\
 Interrupts.cpp\
 Metrics.cpp\
 Params.cpp\
 Pins.cpp\
 Prompt.cpp\
 Schedule.cpp\
 Strategy.cpp\
 System.cpp \
 Tasks.cpp\
 Timers.cpp

HDRS = $(SRCS:.cpp=.h)\
 Codes.h\
 Encoder.h\
 Decoder.h\
 efi_data.h\
 efi_id.h\
 efi_types.h

CFLAGS=-g -O2 -Wall -Wpedantic -Wno-variadic-macros -Wdouble-promotion -pg -DDEBUG
CFLAGS=-g -O2 -Wall -pg -DDEBUG

all:
	make clean
	make efi

efi: $(SRCS) $(HDRS) Makefile
	g++ $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f efi gmon.out

gprof:
	gprof efi

grind:
	valgrind efi

count:
	wc -l *.h
	wc -l *.cpp
