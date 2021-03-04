/*
 * Experimental program to toggle TMS and GPIOL0 signals.
 * Toggles signals at full speed until SIGINT (^C).
 *
 * Results: scope shows both TMS and GPIOL0 are toggling,
 * demonstrating that TMS can be controlled on-the-fly
 * without re-init of bit mode, etc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include "ftd2xx.h"

#define IODIR	0b00011000	// CS=out, GPIOL0=out
#define IOINIT	0b00000000	// CS=low (on), GPIOL0=low
#define IOCSG0	0b00011000	// TMS and GPIOL0 signals, to toggle
// bit   wire           SPI func
//  0    ORN "TCK"      SCLK
//  1    YEL "TDI"      MOSI
//  2    GRN "TDO"      MISO
//  3    BRN "TMS"      /CS (/SS, /SCS)
//  4    GRY "GPIOL0"   -
//  5    VIO "GPIOL1"   -
//  6    WHT "GPIOL2"   -
//  7    BLU "GPIOL3"   -

int run = 1;

void sigact(int signo) {
	run = 0;
}

void dump_buf(unsigned char *buf, int off, int len) {
	int j, k;

	for (j = 0; j < len; j += 16) {
		// TODO: allow custom line prefix
		printf("%04x:", j + off);
		for (k = 0; k < 16 && k + j < len; ++k) {
			printf(" %02x", buf[j + k]);
		}
		while (k < 16) {
			printf("   ");
			++k;
		}
		printf("  ");
		for (k = 0; k < 16 && k + j < len; ++k) {
			int c = buf[j + k];
			if (c < ' ' || c > '~') c = '.';
			printf("%c", c);
		}
		printf("\n");
	}
}

// For now, this serves as 'errno'
FT_STATUS ftStatus = FT_OK;
DWORD driverVersion = 0;

// Normally, only open, close, and xfer are used, but provide access anyway...

// Raw write to device, returns bytes written.
int spi_write(FT_HANDLE ftHandle, unsigned char *buf, const int len) {
	// assert(len < 0x10000);
	ftStatus = FT_OK;
	DWORD bytesToWrite = (DWORD)len;
	DWORD bytesWritten = 0;

	ftStatus = FT_Write(ftHandle, buf, bytesToWrite, &bytesWritten);
	if (ftStatus != FT_OK) {
		//fprintf(stderr, "Failure.  FT_Write returned %d\n", (int)ftStatus);
		// TODO: set errno?
		return -1;
	}
	// This only starts the write... caller polls for read complete...
	return (int)bytesWritten;
}

static int spi_toggle(FT_HANDLE ftHandle) {
	static unsigned char setup[] = {
		0x80, IOINIT, IODIR
	};
	setup[1] ^= IOCSG0;
	int n = spi_write(ftHandle, setup, sizeof(setup));
	if (n < 0 || n != sizeof(setup)) {
		return -1;
	}
	return 0;
}

int spi_setup(FT_HANDLE ftHandle) {
	int n;
	ftStatus = FT_SetBitMode(ftHandle, IODIR, FT_BITMODE_MPSSE);
	if (ftStatus != FT_OK) {
		return -1;
	}
	// assert(len < 0x10000);
	// TODO: how much of this is "once only"?
	unsigned char setup[3] = {
		0x80, IOINIT, IODIR
	};
	unsigned char loopback[1] = {	
		0x85	// loopback off
	};
	unsigned char clock[3] = {	
		0x86, 0x04, 0x00  // TCK divisor: CLK = 6 MHz / (1 + 0004) == 1.2 MHz
	};
	n = spi_write(ftHandle, setup, sizeof(setup));
	if (n < 0 || n != sizeof(setup)) {
		return -1;
	}
	n = spi_write(ftHandle, loopback, sizeof(loopback));
	if (n < 0 || n != sizeof(loopback)) {
		return -1;
	}
	n = spi_write(ftHandle, clock, sizeof(clock));
	if (n < 0 || n != sizeof(clock)) {
		return -1;
	}
	return 0;
}

FT_HANDLE spi_open(int port) {
	FT_HANDLE ftHandle = NULL;
	ftStatus = FT_Open(port, &ftHandle);
	if (ftStatus != FT_OK || ftHandle == NULL) {
		return NULL;
	}
	ftStatus = FT_GetDriverVersion(ftHandle, &driverVersion);
	if (ftStatus != FT_OK) {
		// ignore?
	}
	ftStatus = FT_ResetDevice(ftHandle);
	if (ftStatus != FT_OK) {
		// TODO: need to close?
		goto err_out;
	}
	ftStatus = FT_SetBitMode(ftHandle, 0x00, FT_BITMODE_RESET);
	if (ftStatus != FT_OK) {
		goto err_out;
	}
	ftStatus = FT_SetLatencyTimer(ftHandle, 2);
	if (ftStatus != FT_OK) {
		goto err_out;
	}
	ftStatus = FT_SetTimeouts(ftHandle, 3000, 3000);
	if (ftStatus != FT_OK) {
		goto err_out;
	}
	return ftHandle;
err_out:
	(void)FT_SetBitMode(ftHandle, 0x00, FT_BITMODE_RESET);
	FT_Close(ftHandle);
	return NULL;
}

void spi_close(FT_HANDLE ftHandle) {
	if (ftHandle != NULL) {
		(void)FT_SetBitMode(ftHandle, 0x00, FT_BITMODE_RESET);
		FT_Close(ftHandle);
	}
}

/*
 * Usage: toggle [-p port]
 *
 *
 *
 */
int main(int argc, char **argv) {
	int port = 0;
	int x;
	int c;
	FT_HANDLE ft;
	struct sigaction sa;

	extern char *optarg;
	extern int optind;

	while ((c = getopt(argc, argv, "p:")) != EOF) {
		switch(c) {
		case 'p':
			port = strtol(optarg, NULL, 0);
			break;
		default:
			fprintf(stderr, "Unknown option '%c'\n", c);
			exit(1);
		}
	}
	sa.sa_handler = sigact;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESETHAND;
	x = sigaction(SIGINT, &sa, NULL);
	if (x < 0) {
		perror("sigaction");
		exit(1);
	}
	ft = spi_open(port);
	if (ft == NULL) {
		fprintf(stderr, "Unable to open device, error = %d\n", ftStatus);
		exit(1);
	}
	x = spi_setup(ft);
	if (x < 0) {
		fprintf(stderr, "Failure during setup, error = %d\n", ftStatus);
		exit(1);
	}
	while (run) {
		x = spi_toggle(ft);
		if (x < 0) {
			fprintf(stderr, "Failure during toggle, error = %d\n", ftStatus);
			exit(1);
		}
	}
	spi_close(ft);
	return 0;
}
