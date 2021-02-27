# Makefile for FTDI MPSSE example programs
# using libftd2xx
# Assumes libftd2xx installed in /usr/local/

TOP = /usr/local/ftd2xx

CFLAGS = -I $(TOP)/include -L$(TOP)/lib
# TODO: get dynamic lib working
FTDLIB = -lftd2xx

all: jtag

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(FTDLIB) -lpthread -Wl,-rpath $(TOP)/lib
