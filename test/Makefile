# Makefile for FTDI MPSSE example programs
# using libftd2xx

LIBFTD2XX_DIR = ~/foreign/libftd2xx/release

CFLAGS = -I $(LIBFTD2XX_DIR)
# TODO: get dynamic lib working
FTDLIB = $(LIBFTD2XX_DIR)/build/libftd2xx.a

all: jtag

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(FTDLIB) -lpthread
