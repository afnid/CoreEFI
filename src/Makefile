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
	mkdir -p efif4/src
	ln -sf $(PWD)/*.cpp efif4/src
	ln -sf $(PWD)/*.h efif4/src
	ln -sf $(PWD)/Makefile.f4 efif4/Makefile

efil1:
	mkdir -p efil1/src
	ln -sf $(PWD)/*.cpp efil1/src
	ln -sf $(PWD)/*.h efil1/src
	ln -sf $(PWD)/Makefile.l1 efil1/Makefile

efil0:
	mkdir -p efil0/src
	ln -sf $(PWD)/*.cpp efil0/src
	ln -sf $(PWD)/*.h efil0/src
	ln -sf $(PWD)/Makefile.l0 efil0/Makefile
