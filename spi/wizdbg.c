/*
 * General-purpose read/write for WIZ850io (W5500)
 *
 * Usage: wizdbg [options] <bsb> <off> <len>
 *        wizdbg [options] [-w] <bsb> <off> <byte>[...]
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "ftd2xx.h"
#include "spilib.h"

int main(int argc, char **argv) {
	int wr = 0;
	int bsb = 0;
	int off = 0;
	int len = 0;
	int tot = 0;
	int port = 0;
	int speed = 0;
	int verbose = 0;
	int cs = 'C';
	unsigned char *bufo;
	unsigned char *bufi;
	int x;
	int c;
	int e;
	FT_HANDLE ft;

	extern char *optarg;
	extern int optind;

	while ((c = getopt(argc, argv, "g:p:s:vw")) != EOF) {
		switch(c) {
		case 'g':
			cs = set_cs(optarg[0]);
			if (cs < 0) {
				fprintf(stderr, "Invalid GPIO /CS\n");
				exit(1);
			}
			break;
		case 'p':
			port = strtol(optarg, NULL, 0);
			break;
		case 's':
			speed = parse_speed(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			wr = 1;
			break;
		default:
			fprintf(stderr, "Unknown option '%c'\n", c);
			exit(1);
		}
	}
	e = 0; // command parse error?
	if (wr) {
		e = (argc - optind < 3);
	} else {
		e = (argc - optind != 3);
	}
	if (e) {
		fprintf(stderr, "Usage: %s [options] <bsb> <off> <len>\n", argv[0]);
		fprintf(stderr, "       %s [options] -w <bsb> <off> <byte>[...]\n", argv[0]);
		fprintf(stderr, "Options:\n"
				"    -p port  Use port instead of 0\n"
				"    -s hz   Use hz clock speed (def 1.2M)\n"
				"    -g cs   Use gpio for chip-select (0..3, def C)\n"
		);
		exit(1);
	}
	if (speed > 0) {
		speed = spi_speed(speed);
	} else {
		speed = spi_speed(0);
	}
	if (verbose) {
		printf("Using speed %sHz\n", print_speed(speed));
		printf("Using chip-select '%c'\n", cs);
	}
	x = optind;
	bsb = strtol(argv[x++], NULL, 0);
	off = strtol(argv[x++], NULL, 0);
	// READ and WRITE commands are always 3 bytes.
	if (wr) {
		len = argc - x;
	} else {
		len = strtol(argv[x++], NULL, 0);
	}
	if (len > 61) {
		fprintf(stderr, "Hardware limited to <= 61 byte transfers\n");
		exit(1);
	}
	tot = len + 3;
	bufo = malloc(tot);
	bufi = malloc(tot);
	if (bufo == NULL || bufi == NULL) {
		fprintf(stderr, "Out of memory, 2x%d bytes\n", tot);
		exit(1);
	}
	bufo[0] = (off >> 8) & 0xff; // big-endian address
	bufo[1] = off & 0xff;
	bufo[2] = (bsb << 3); // READ
	if (wr) {
		bufo[2] |= 0x04; // WRITE
		int y = 3;
		while (x < argc) {
			bufo[y++] = (unsigned char)strtol(argv[x], NULL, 0);
			++x;
		}
	} else {
		// Read data is sent as FF...
		memset(bufo + 3, 0xff, len);
	}
	ft = spi_open(port);
	if (ft == NULL) {
		fprintf(stderr, "Unable to open device, error = %d\n", ftStatus);
		exit(1);
	}
	if (wr) {
		e = spi_xfer(ft, bufo, bufi, tot);
	} else {
		e = spi_xfer(ft, bufo, bufi, tot);
		if (e >= 0) {
			dump_buf2(bufi + 3, bsb, off, len);
		}
	}
	if (e < 0) {
		fprintf(stderr, "Failure during transfer, error = %d\n", ftStatus);
	}
	spi_close(ft);
	return 0;
}
