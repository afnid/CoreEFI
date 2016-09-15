SRCS:=$(shell echo *.cpp)
HDRS:=$(shell echo *.h)

CFLAGS=-g -O2 -Wall -Wpedantic -Wno-variadic-macros -Wdouble-promotion -pg -DDEBUG
CFLAGS=-g -O4 -Wall -Wdouble-promotion -pg -DDEBUG
CFLAGS=-g -O4 -Wall -pg -DDEBUG

CFLAGS = -g -O2 -Wall -Wpedantic
CFLAGS += -DDEBUG
CFLAGS += -I. -Istd
CFLAGS += -fsingle-precision-constant
CFLAGS += -flto -ffunction-sections -fdata-sections -fno-builtin
CFLAGS += -fno-rtti -fno-exceptions -ffast-math
CFLAGS += -fpermissive -fno-threadsafe-statics -std=gnu++11
CFLAGS += -Wl,--gc-sections

all:
	@ if [ -d efil1 ]; then echo "-------" efil1; cd efil1; make; fi
	@ if [ -d efil0 ]; then echo "-------" efil0; cd efil0; make; fi
	@ if [ -d efif4 ]; then echo "-------" efif4; cd efif4; make; fi
	# make clean
	make efi

efi: $(SRCS) $(HDRS) Makefile
	g++ $(CFLAGS) $(SRCS) -o $@
	@size $@

clean:
	rm -f efi gmon.out
	rm -rf efil0 efil1 efif4

gprof:
	gprof efi

grind:
	valgrind efi

count:
	wc -l *.h
	wc -l *.cpp

efif4:
	st/cross.sh f4 efi

efil1:
	st/cross.sh l1 efi

efil0:
	st/cross.sh l0 efi
