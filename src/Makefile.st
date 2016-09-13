STM32=~/bin/stm32.py

.PHONY: proj

all:: cube proj

cube: 
	$(STM32) $(STDIR) $(STINC)
	@ln -sf $(PWD)/cube $(PWD)/cube/Legacy
	rm -rf cube/*template*
	@exit

STLINK=~/dev/stm32/texane/stlink/build
EABI=~/dev/stm32/gcc-arm-none-eabi-5_4-2016q2

AR=$(EABI)/bin/arm-none-eabi-ar
CPP=$(EABI)/bin/arm-none-eabi-g++
CC=$(EABI)/bin/arm-none-eabi-gcc
OBJCOPY=$(EABI)/bin/arm-none-eabi-objcopy
SIZE=$(EABI)/bin/arm-none-eabi-size

CFLAGS += -g -Os -Wall
CFLAGS += -mlittle-endian -mthumb -mthumb-interwork
CFLAGS += -fsingle-precision-constant -Wdouble-promotion
CFLAGS += -flto -ffunction-sections -fdata-sections -fno-builtin
CFLAGS += -fno-rtti -fno-exceptions -ffast-math
CFLAGS += -specs=nosys.specs
CFLAGS += -DDEBUG -DSTM32
CFLAGS += -Isrc -Icube

CPPFLAGS = $(CFLAGS)
CPPFLAGS += -fpermissive -fno-threadsafe-statics -std=gnu++11
CPPFLAGS += -Wl,--static -Wl,--gc-sections
CPPFLAGS += -Wl,-Map=$(PROJ).map

PROJ=main

proj: $(PROJ).elf

STSRCS := $(shell find cube/ -name "*.c")
STHDRS := $(shell find cube/ -name "*.h")

SRCS:=$(shell echo src/*.cpp)
HDRS:=$(shell echo src/*.h)

$(PROJ).elf: cube Makefile $(HDRS) $(STHDRS) $(SRCS) $(STSRCS)
	@echo $(CPP) $(CPPFLAGS) -o $@
	@$(CPP) $(CPPFLAGS) $(SRCS) $(STSRCS) -o $@
	@$(OBJCOPY) -O ihex $@ $(PROJ).hex
	@$(OBJCOPY) -O binary $@ $(PROJ).bin
	@$(SIZE) $@

clean:
	rm -f $(PROJ) $(PROJ).*

realclean: clean
	rm -rf cube

burn: proj
	usurp $(STLINK)/st-flash write $(PROJ).bin 0x8000000

term:
	-killall gtkterm
	gtkterm -p /dev/ttyUSB0& 

term0:
	gtkterm -p /dev/ttyUSB0& 

term1:
	gtkterm -p /dev/ttyUSB1& 

term2:
	gtkterm -p /dev/ttyUSB2& 
