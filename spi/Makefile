# Makefile for FTDI MPSSE SPI programs
# using libftd2xx
# Assumes libftd2xx installed in /usr/local/ftd2xx

TOP = /usr/local/ftd2xx

CFLAGS = -I $(TOP)/include -I. -L$(TOP)/lib
# TODO: get dynamic lib working
FTDLIB = -lftd2xx

all: spidbg nvram wizdbg

%.o: %.c spilib.h
	$(CC) $(CFLAGS) -c -o $@ $<

toggle: toggle.c
	$(CC) $(CFLAGS) -o $@ $^ $(FTDLIB) -lpthread -Wl,-rpath $(TOP)/lib

NVRAM = nvram.o spilib.o
WIZDBG = wizdbg.o spilib.o
SPIDBG = spidbg.o spilib.o crc16.o

spidbg: $(SPIDBG)
	$(CC) $(CFLAGS) -o $@ $^ $(FTDLIB) -lpthread -Wl,-rpath $(TOP)/lib

nvram: $(NVRAM)
	$(CC) $(CFLAGS) -o $@ $^ $(FTDLIB) -lpthread -Wl,-rpath $(TOP)/lib

wizdbg: $(WIZDBG)
	$(CC) $(CFLAGS) -o $@ $^ $(FTDLIB) -lpthread -Wl,-rpath $(TOP)/lib
