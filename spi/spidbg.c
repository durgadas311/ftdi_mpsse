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

#undef USE_WREN

int main(int argc, char **argv) {
	int len;
	int port = 0;
	unsigned char *bufo;
	unsigned char *bufi;
	int x;
	int c;
	FT_HANDLE ft;
#ifdef USE_WREN
	int wren = 0;
	unsigned char wbuf[] = { 0x06 };
	unsigned char wxbuf[1];
	unsigned char sbuf[] = { 0x05, 0xff };
	unsigned char sxbuf[2];
#endif

	extern char *optarg;
	extern int optind;

	len = -1;
	while ((c = getopt(argc, argv, "l:p:w")) != EOF) {
		switch(c) {
		case 'l':
			len = strtol(optarg, NULL, 0);
			break;
		case 'p':
			port = strtol(optarg, NULL, 0);
			break;
#ifdef USE_WREN
		case 'w':
			wren = 1;
			break;
#endif
		default:
			fprintf(stderr, "Unknown option '%c'\n", c);
			exit(1);
		}
	}
	if (argc - optind < 1) {
		fprintf(stderr, "Usage: %s [-p port][-l len] <byte>[...]\n", argv[0]);
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
	ft = spi_open(port);
	if (ft == NULL) {
		fprintf(stderr, "Unable to open device, error = %d\n", ftStatus);
		exit(1);
	}
#ifdef USE_WREN
	if (wren) {
		x = spi_xfer(ft, wbuf, wxbuf, sizeof(wbuf));
		if (x < 0) {
			fprintf(stderr, "Failure during WREN, error = %d\n", ftStatus);
		}
	}
#endif
	x = spi_xfer(ft, bufo, bufi, len);
	if (x < 0) {
		fprintf(stderr, "Failure during transfer, error = %d\n", ftStatus);
	} else {
#ifdef USE_WREN
		if (wren) {
			x = spi_xfer(ft, sbuf, sxbuf, sizeof(sbuf));
			if (x < 0) {
				fprintf(stderr, "Failure during READ STATUS, error = %d\n", ftStatus);
			}
			printf("WREN:\n");
			dump_buf(wbuf, sizeof(wbuf));
			dump_buf(wxbuf, sizeof(wxbuf));
		}
#endif
		printf("Write:\n");
		dump_buf(bufo, len);
		printf("Read (%06x %d):\n", driverVersion, ftStatus);
		dump_buf(bufi, len);
#ifdef USE_WREN
		if (wren) {
			printf("STATUS:\n");
			dump_buf(sbuf, sizeof(sbuf));
			dump_buf(sxbuf, sizeof(sxbuf));
		}
#endif
	}
	spi_close(ft);
	return 0;
}
