# Name: Makefile
# Author: Nathan Witmer <nathan@zerowidth.com>
# Copyright: 2015 Nathan Witmer
# License: MIT

DEVICE     = attiny84a
AVR_DEVICE = t84
CLOCK      = 8000000
PROGRAMMER = -c usbtiny
OBJECTS    = main.o softuart.o
# 0xe2 for internal 8MHz clock, 0x62 for internal 1MHz:
FUSES      = -U lfuse:w:0xe2:m -U hfuse:w:0xdf:m # -U efuse:w:0xff:m

AVRDUDE = avrdude $(PROGRAMMER) -p $(AVR_DEVICE)
COMPILE = avr-gcc -Wall -Os -flto -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)

all:	build size

run:	build size flash

build:	main.hex

%.o: %.c %.h
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
	# "-x assembler-with-cpp" should not be necessary since this is the default
	# file type for the .S (with capital S) extension. However, upper case
	# characters are not always preserved on Windows. To ensure WinAVR
	# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@

size:	main.elf
	avr-size --format=avr --mcu=$(DEVICE) main.elf

flash:	main.hex
	$(AVRDUDE) -U flash:w:main.hex:i

fuse:
	$(AVRDUDE) $(FUSES)

# Xcode uses the Makefile targets "", "clean" and "install"
install: flash fuse

# if you use a bootloader, change the command below appropriately:
load: all
	bootloadHID main.hex

clean:
	rm -f main.hex main.elf $(OBJECTS)

# file targets:
main.elf: $(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS)

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
# If you have an EEPROM section, you must also create a hex file for the
# EEPROM and add it to the "flash" target.

# Targets for code debugging and analysis:
disasm:	main.elf
	avr-objdump -d main.elf

cpp:
	$(COMPILE) -E main.c
