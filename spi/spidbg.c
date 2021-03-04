/*
 * General-purpose debug command for SPI transfers.
 *
 * Usage: spidbg [-p port][-l len] <byte>[...]
 *
 * 'len' must be at least "<byte>[...]" count.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "ftd2xx.h"
#include "spilib.h"

extern unsigned short crc16(unsigned char *buf, int len);

int main(int argc, char **argv) {
	int tot;
	int cmd;
	int len = 0;
	int port = 0;
	int crc = 0;
	int verbose = 0;
	unsigned char *bufo;
	unsigned char *bufi;
	int x;
	int c;
	FT_HANDLE ft;

	extern char *optarg;
	extern int optind;

	while ((c = getopt(argc, argv, "cl:p:v")) != EOF) {
		switch(c) {
		case 'c':
			crc = 1;
			break;
		case 'l':
			len = strtol(optarg, NULL, 0);
			break;
		case 'p':
			port = strtol(optarg, NULL, 0);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "Unknown option '%c'\n", c);
			exit(1);
		}
	}
	if (argc - optind < 1) {
		fprintf(stderr, "Usage: %s [-v|-c][-p port][-l len] <byte>[...]\n", argv[0]);
		fprintf(stderr, "Where:\n"
				"    -l len  Read len additional bytes\n"
				"    -c      Print only CRC16 of read data (req -l)\n"
				"    -v      Print full write and read buffers (ovr -c)\n"
				"    -p port Use port instead of 0\n"
		);
		exit(1);
	}
	cmd = argc - optind;
	tot = cmd + len;
	bufo = malloc(tot);
	bufi = malloc(tot);
	if (bufo == NULL || bufi == NULL) {
		fprintf(stderr, "Out of memory, 2x%d bytes\n", tot);
		exit(1);
	}
	for (x = 0; x < cmd; ++x) {
		bufo[x] = (unsigned char)strtol(argv[optind + x], NULL, 0);
	}
	if (len > 0) {
		// Read data is sent as FF...
		memset(bufo + cmd, 0xff, len);
	}
	ft = spi_open(port);
	if (ft == NULL) {
		fprintf(stderr, "Unable to open device, error = %d\n", ftStatus);
		exit(1);
	}
	x = spi_xfer(ft, bufo, bufi, tot);
	if (x < 0) {
		fprintf(stderr, "Failure during transfer, error = %d\n", ftStatus);
	} else if (verbose) {
		printf("Write:\n");
		dump_buf(bufo, 0, tot);
		printf("Read (%06x %d):\n", driverVersion, ftStatus);
		dump_buf(bufi, 0, tot);
	} else if (len > 0) {
		if (crc) {
			printf("CRC: %04x\n", crc16(bufi + cmd, len));
		} else {
			dump_buf(bufi + cmd, 0, len);
		}
	}
	spi_close(ft);
	return 0;
}
