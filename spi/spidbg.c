/*
 * General-purpose debug command for SPI transfers.
 *
 * Usage: spidbg [-l len] <byte>[...]
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

int main(int argc, char **argv) {
	int len;
	unsigned char *bufo;
	unsigned char *bufi;
	int x;
	int c;

	extern char *optarg;
	extern int optind;

	len = -1;
	while ((c = getopt(argc, argv, "l:")) != EOF) {
		switch(c) {
		case 'l':
			len = strtol(optarg, NULL, 0);
			break;
		default:
			fprintf(stderr, "Unknown option '%c'\n", c);
			exit(1);
		}
	}
	if (argc - optind < 1) {
		fprintf(stderr, "Usage: %s [-l len] <byte>[...]\n", argv[0]);
		fprintf(stderr, "Where 'len' must be at least <byte>[...] length\n");
		exit(1);
	}
	if (len <= 0) {
		len = argc - optind;
	}
	bufo = malloc(len);
	bufi = malloc(len);
	if (bufo == NULL || bufi == NULL) {
		fprintf(stderr, "Out of memory, 2x%d bytes\n", len);
		exit(1);
	}
	for (x = 0; x < len; ++x) {
		if (optind + x < argc) {
			bufo[x] = (unsigned char)strtol(argv[optind + x], NULL, 0);
		} else {
			bufo[x] = (unsigned char)0xff;
		}
	}
	if (1) {
		printf("Write:\n");
		dump_buf(bufo, len);
	}
	// TODO: perform operation...
	FT_HANDLE ft;
	ft = spi_open(0); // TODO: allow choice of port
	if (ft == NULL) {
		fprintf(stderr, "Unable to open device, error = %d\n", ftStatus);
		exit(1);
	}
	x = spi_xfer(ft, bufo, bufi, len);
	if (x < 0) {
		fprintf(stderr, "Failure during transfer , error = %d\n", ftStatus);
	} else {
		printf("Read (%06x %d):\n", driverVersion, ftStatus);
		dump_buf(bufi, len);
	}
	spi_close(ft);
	return 0;
}
