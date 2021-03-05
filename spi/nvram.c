/*
 * General-purpose read/write for 25LC512 SEEPROM
 *
 * Usage: nvram [-p port] <addr> <len>
 *        nvram [-p port][-w] <addr> <byte>[...]
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "ftd2xx.h"
#include "spilib.h"

static int nvwrite(FT_HANDLE ft, unsigned char *bufo, unsigned char *bufi, int len) {
	unsigned char wren[] = { 0x06 };
	unsigned char wrdi[] = { 0x04 };
	unsigned char stat[] = { 0x05, 0xff };
	unsigned char buf[2];

	int e = spi_xfer(ft, wren, buf, sizeof(wren));
	if (e < 0) return -1;
	// 
	e = spi_xfer(ft, bufo, bufi, len);
	if (e < 0) return -2;
	do {
		e = spi_xfer(ft, stat, buf, sizeof(stat));
		if (e < 0) return -3;
	} while ((buf[1] & 0x01) != 0);
	if ((buf[1] & 0x02) != 0) {
		(void)spi_xfer(ft, wrdi, buf, sizeof(wrdi));
		ftStatus = -1;
		return -4;
	}
	return 0;
}

int main(int argc, char **argv) {
	int wr = 0;
	int addr = 0;
	int len = 0;
	int tot = 0;
	int port = 0;
	unsigned char *bufo;
	unsigned char *bufi;
	int x;
	int c;
	int e;
	FT_HANDLE ft;

	extern char *optarg;
	extern int optind;

	while ((c = getopt(argc, argv, "p:w")) != EOF) {
		switch(c) {
		case 'p':
			port = strtol(optarg, NULL, 0);
			break;
		case 'w':
			wr = 1;
			break;
		default:
			fprintf(stderr, "Unknown option '%c'\n", c);
			exit(1);
		}
	}
	if (argc - optind < 2) {
		fprintf(stderr, "Usage: %s [-p port] <addr> <len>\n", argv[0]);
		fprintf(stderr, "       %s [-p port] -w <addr> <byte>[...]\n", argv[0]);
		exit(1);
	}
	x = optind;
	addr = strtol(argv[x++], NULL, 0);
	// READ and WRITE commands are always 3 bytes.
	if (wr) {
		len = argc - x;
		// WRITE command can only be 128+3, max
		tot = 128 + 3;
	} else {
		len = strtol(argv[x++], NULL, 0);
		tot = len + 3;
	}
	bufo = malloc(tot);
	bufi = malloc(tot);
	if (bufo == NULL || bufi == NULL) {
		fprintf(stderr, "Out of memory, 2x%d bytes\n", tot);
		exit(1);
	}
	if (wr) {
		// scan argv 128 bytes at a time, when we're ready to send commands...
		bufo[0] = 0x02;
		bufo[1] = (addr >> 8) & 0xff;
		bufo[2] = addr & 0xff;
	} else {
		// Read data is sent as FF...
		memset(bufo + 3, 0xff, len);
		bufo[0] = 0x03;
		bufo[1] = (addr >> 8) & 0xff;
		bufo[2] = addr & 0xff;
	}
	ft = spi_open(port);
	if (ft == NULL) {
		fprintf(stderr, "Unable to open device, error = %d\n", ftStatus);
		exit(1);
	}
	if (wr) {
		e = 0;
		while (e >= 0 && x < argc) {
			int y = 0;
			while (x + y < argc && y < 128) {
				bufo[y + 3] = (unsigned char)strtol(argv[x + y], NULL, 0);
				++y;
			}
			// y == length of data
			e = nvwrite(ft, bufo, bufi, y + 3);
			x += y;
		}
	} else {
		e = spi_xfer(ft, bufo, bufi, tot);
		if (e >= 0) {
			dump_buf(bufi + 3, addr, len);
		}
	}
	if (e < 0) {
		fprintf(stderr, "Failure during transfer, error = %d\n", ftStatus);
	}
	spi_close(ft);
	return 0;
}
