/*
 * Enable Multi-Protocol Synchronous Serial Engine (MPSSE) on an FTDI chip,
 * and use SPI commands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ftd2xx.h"

#define IODIR	0b1011	// CS=out, TDO=in, TDI=out, TCK=out
#define IOINIT	0b1000	// CS=high (off), SCLK=low
#define IO_CS	0b1000	// CS bit location/mask
// bit   wire           SPI func
//  0    ORN "TCK"      SCLK
//  1    YEL "TDI"      MOSI
//  2    GRN "TDO"      MISO
//  3    BRN "TMS"      /CS (/SS, /SCS)
//  4    GRY "GPIOL0"   -
//  5    VIO "GPIOL1"   -
//  6    WHT "GPIOL2"   -
//  7    BLU "GPIOL3"   -

#define DEBUG

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

static void spi_flush(FT_HANDLE ftHandle) {
	DWORD bytesReceived = 0;
	DWORD bytesRead = 0;
	unsigned char *buf;
	ftStatus = FT_GetQueueStatus(ftHandle, &bytesReceived);
	if (ftStatus != FT_OK || bytesReceived == 0) {
		return;
	}
	buf = malloc(bytesReceived);
	if (buf == NULL) {
		perror("malloc");
		return;
	}
	ftStatus = FT_Read(ftHandle, buf, bytesReceived, &bytesRead);
	if (ftStatus == FT_OK) {
		// if (bytesReceived != bytesRead) ...
		dump_buf(buf, 0xf000, bytesRead);
	}
	free(buf);
}

// Set /CS on (low) or off (high).
int spi_cs(FT_HANDLE ftHandle, int on) {
	unsigned char setup[] = {
		0x80, IOINIT, IODIR
	};
	if (on) {
		setup[1] &= ~0b1000; // clear bit = on, active low signal
	} else {
		setup[1] |= 0b1000; // set bit = off, active low signal
	}
	int n = spi_write(ftHandle, setup, sizeof(setup));
	if (n < 0 || n != sizeof(setup)) {
		return -1;
	}
	return 0;
}

int spi_setup(FT_HANDLE ftHandle) {
	int n;
	unsigned char setup[3] = {
		0x80, IOINIT, IODIR
	};
	unsigned char loopback[1] = {
		0x85	// loopback off
	};
	unsigned char clock[3] = {
		0x86, 0x04, 0x00  // TCK divisor: CLK = 6 MHz / (1 + 0004) == 1.2 MHz
	};
	ftStatus = FT_SetBitMode(ftHandle, IODIR, FT_BITMODE_MPSSE);
	if (ftStatus != FT_OK) {
		return -1;
	}
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

// Send SPI write prefix - prepare to send data to SPI device
// (not commands to C232HM).
static int spi_prep(FT_HANDLE ftHandle, DWORD len) {
	unsigned char xfer[3] = {
		0x31, 0x00, 0x00  // Write + read; length bytes set below.
	};
	xfer[1] = (unsigned char)(len & 0x00FF);
	xfer[2] = (unsigned char)((len & 0xFF00) >> 8);
	int n = spi_write(ftHandle, xfer, sizeof(xfer));
	if (n < 0 || n != sizeof(xfer)) {
		return -1;
	}
	return 0;
}

// Generally, must be preceeded by spi_write().
static int spi_wait(FT_HANDLE ftHandle, int len) {
	struct timeval startTime;
	int journeyDuration;
	DWORD bytesReceived = 0;
	int queueChecks = 0;

	// assert(len < 0x10000);
	journeyDuration = 1;  // One second should be enough
	gettimeofday(&startTime, NULL);
	queueChecks = 0;
	for (bytesReceived = 0; bytesReceived < len; queueChecks++) {
		if (queueChecks % 512 == 0) {
			struct timeval now;
			struct timeval elapsed;
			gettimeofday(&now, NULL);
			timersub(&now, &startTime, &elapsed);
			if (elapsed.tv_sec > (long int)journeyDuration) {
				break;
			}
			// TODO: progress indicator...
		}
		ftStatus = FT_GetQueueStatus(ftHandle, &bytesReceived);
		if (ftStatus != FT_OK) {
			return -1;
		}
	}
	// This appears to be enough to fix some timing glitch...
	// Theat caused issues with the 25LC512 nvram, but glitching
	// more clocks after this point, in FT_Read()?
	(void)FT_GetQueueStatus(ftHandle, &bytesReceived);
	return (int)bytesReceived;
}

static int spi_get(FT_HANDLE ftHandle, unsigned char *buf, int len) {
	DWORD bytesRead = 0;
	ftStatus = FT_Read(ftHandle, buf, len, &bytesRead);
	if (ftStatus != FT_OK) {
		return -1;
	}
	return (int)bytesRead;
}

// Read as many bytes as are available, at least 'len'
int spi_read(FT_HANDLE ftHandle, unsigned char *buf, int len) {
	int m;
	int n = spi_wait(ftHandle, len);
#ifdef DEBUG
	if (n < 0) {
		return -1;
	}
	(void)spi_cs(ftHandle, 0); // /CS off
	if (n != len) {
		printf("Sent %d, got back %d\n", len, n);
		if (n > len) {
			unsigned char *b = malloc(n);
			if (b == NULL) return -1;
			m = spi_get(ftHandle, b, n);
			memcpy(buf, b, len);
			dump_buf(b, 0xf000, n);
			free(b);
		} else {
			m = spi_get(ftHandle, buf, n);
			dump_buf(buf, 0xf000, n);
		}
	} else {
		m = spi_get(ftHandle, buf, len);
	}
#else // !DEBUG
	(void)spi_cs(ftHandle, 0); // /CS off
	int l = len;
	if (n != len) {
		if (n < len) l = n;
		m = spi_get(ftHandle, buf, l);
		(void)FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
		fprintf(stderr, "Sent %d, got back %d\n", len, n);
		//return -1;
	}
	m = spi_get(ftHandle, buf, len);
#endif // !DEBUG
	return m;
}

// Start a command sequence (/CS activated)
int spi_begin(FT_HANDLE ftHandle, int len) {
	int n = spi_cs(ftHandle, 1); // /CS on
	if (n < 0) {
		return -1;
	}
	n = spi_prep(ftHandle, len); // setup write
	if (n < 0) {
		return -1;
	}
	return 0;
}

int spi_end(FT_HANDLE ftHandle) {
	int n = spi_cs(ftHandle, 0); // /CS off
	if (n < 0) {
		return -1;
	}
	return 0;
}

// Returns bytes read, or -1 on error.
int spi_xfer(FT_HANDLE ftHandle, unsigned char *bufout,
			unsigned char *bufin, const int len) {
	int n = spi_begin(ftHandle, len); // setup write
	if (n < 0) {
		return -1;
	}
	n = spi_write(ftHandle, bufout, len);
	if (n < 0 || n != len) {
		return -1;
	}
	n = spi_read(ftHandle, bufin, len);
	int m = spi_end(ftHandle);
	if (m < 0) {
		return -1;
	}
	return n;
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
	int n = spi_setup(ftHandle);
	if (n < 0) {
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
