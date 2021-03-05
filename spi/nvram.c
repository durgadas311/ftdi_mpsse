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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "ftd2xx.h"
#include "spilib.h"

static unsigned char wrbuf[3 + 128] = { 0 };
static unsigned char wxbuf[3 + 128];
static unsigned char wren[] = { 0x06 };
static unsigned char wrdi[] = { 0x04 };
static unsigned char rdsr[] = { 0x05, 0xff };
static unsigned char rbuf[2];

static int nvwrite(FT_HANDLE ft, unsigned char *buf, int addr, int len) {
	memcpy(wrbuf + 3, buf, len); // 1 <= len <= 128
	wrbuf[0] = 0x02; // WRITE command
	wrbuf[1] = (addr >> 8) & 0xff; // big-endian address
	wrbuf[2] = addr & 0xff;
	int e = spi_xfer(ft, wren, rbuf, sizeof(wren));
	if (e < 0) return -1;
	// 
	e = spi_xfer(ft, wrbuf, wxbuf, len + 3);
	if (e < 0) return -2;
	do {
		e = spi_xfer(ft, rdsr, buf, sizeof(rdsr));
		if (e < 0) return -3;
	} while ((buf[1] & 0x01) != 0);
	// something went wrong if WREN still set...
	if ((buf[1] & 0x02) != 0) {
		(void)spi_xfer(ft, wrdi, buf, sizeof(wrdi));
		ftStatus = -1;
		return -4;
	}
	return 0;
}

static int nvwrbuf(FT_HANDLE ft, unsigned char *buf, int addr, int len) {
	int e;
	int a = addr;
	int l = len;
	unsigned char *b = buf;
	while (l >= 128) {
		e = nvwrite(ft, b, a, 128);
		if (e < 0) break;
		b += 128;
		a += 128;
		l -= 128;
	}
	if (l > 0) {
		e = nvwrite(ft, b, a, l);
	}
	// TODO: or return len-l?
	return e < 0 ? -1 : len;
}

int main(int argc, char **argv) {
	int wr = 0;
	int addr = 0;
	int len = 0;
	int tot = 0;
	int port = 0;
	int speed = 0;
	int verbose = 0;
	char *file = NULL;
	int fd;
	unsigned char *bufo;
	unsigned char *bufi;
	int x;
	int c;
	int e;
	FT_HANDLE ft;

	extern char *optarg;
	extern int optind;

	while ((c = getopt(argc, argv, "f:p:s:vw")) != EOF) {
		switch(c) {
		case 'f':
			file = optarg;
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
		if (file) e = (argc - optind != 1);
		else e = (argc - optind < 2);
	} else {
		e = (argc - optind != 2);
	}
	if (e) {
		fprintf(stderr, "Usage: %s [options] <addr> <len>\n", argv[0]);
		fprintf(stderr, "       %s [options] -w <addr> <byte>[...]\n", argv[0]);
		fprintf(stderr, "Options:\n"
				"    -p port  Use port instead of 0\n"
				"    -f file  Use file for data (no <byte>[...])\n"
				"    -s hz   Use hz clock speed (def 1.2M)\n"
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
	}
	x = optind;
	addr = strtol(argv[x++], NULL, 0);
	// READ and WRITE commands are always 3 bytes.
	if (wr) {
		if (file) {
			struct stat stb;
			fd = open(file, O_RDONLY);
			if (fd < 0) {
				perror(file);
				exit(1);
			}
			fstat(fd, &stb);
			len = stb.st_size;
			if (len > 65536) len = 65536;
			bufo = malloc(len);
			if (bufo == NULL) {
				perror("malloc");
				exit(1);
			}
			x = read(fd, bufo, len);
			if (x < 0) {
				perror(file);
				exit(1);
			}
			close(fd);
		} else {
			len = argc - x;
			bufo = malloc(len);
			if (bufo == NULL) {
				perror("malloc");
				exit(1);
			}
			int y = 0;
			while (x < argc) {
				bufo[y++] = (unsigned char)strtol(argv[x], NULL, 0);
				++x;
			}
		}
	} else {
		len = strtol(argv[x++], NULL, 0);
		tot = len + 3;
		bufo = malloc(tot);
		bufi = malloc(tot);
		if (bufo == NULL || bufi == NULL) {
			fprintf(stderr, "Out of memory, 2x%d bytes\n", tot);
			exit(1);
		}
		// Read data is sent as FF...
		memset(bufo + 3, 0xff, len);
		bufo[0] = 0x03; // READ command
		bufo[1] = (addr >> 8) & 0xff; // big-endian address
		bufo[2] = addr & 0xff;
		if (file) {
			fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (fd < 0) {
				perror(file);
				exit(1);
			}
		}
	}
	ft = spi_open(port);
	if (ft == NULL) {
		fprintf(stderr, "Unable to open device, error = %d\n", ftStatus);
		exit(1);
	}
	if (wr) {
		e = nvwrbuf(ft, bufo, addr, len);
	} else {
		e = spi_xfer(ft, bufo, bufi, tot);
		if (e >= 0) {
			if (file) {
				x = write(fd, bufi + 3, len);
				if (x < 0) {
					perror(file);
				}
				x = close(fd);
				if (x < 0) {
					perror(file);
				}
			} else {
				dump_buf(bufi + 3, addr, len);
			}
		}
	}
	if (e < 0) {
		fprintf(stderr, "Failure during transfer, error = %d\n", ftStatus);
	}
	spi_close(ft);
	return 0;
}
