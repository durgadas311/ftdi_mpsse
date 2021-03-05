/*
 * Enable Multi-Protocol Synchronous Serial Engine (MPSSE) on an FTDI chip,
 * and use SPI commands.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>
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

// C232HM MPSSE commands
#define	MP_CLKBYTES	0x31	// send/recv bytes, MSB 1st, -ve
#define MP_FLUSH	0x87	// flush bytes to recv queue
#define MP_SETIO	0x80	// set I/O pins, ADBUS (low byte)
#define MP_NOLOOP	0x85	// loopback off
#define MP_CLKDIV	0x86	// set clock divisor
#define MP_DIV5DI	0x8a	// disable clock divide-by-5 prescale (60MHz)
#define MP_DIV5EN	0x8b	// enable clock divide-by-5 prescale (12MHz)

#undef DEBUG

#define CLK_RAW		60000000	// internal clock is 60MHz
#define CLK_MAX		(CLK_RAW / 2)	// 60MHz / 2 is maximum
#define CLK_DIV5	(CLK_RAW / 5)	// default divide-by-5 clock
#define CLK_DMAX	(CLK_DIV5 / 2)	// max for using default 12MHz
// Clock defaults: 12MHz divide by 5 = 1.2MHz
static unsigned char div5[] = {
	MP_DIV5EN
};
static unsigned char setclk[] = {
	MP_CLKDIV, 0x04, 0x00  // TCK divisor: CLK = 6 MHz / (1 + 0004) == 1.2 MHz
};

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

int parse_speed(char *arg) {
	char *end;
	double clk = strtod(arg, &end);
	if (toupper(*end) == 'M') {
		// TODO: *(end+1) == 0
		clk *= 1e6;
	} else if (toupper(*end) == 'K') {
		// TODO: *(end+1) == 0
		clk *= 1e3;
	} else if (*end != 0) {
		return -1;
	}
	if (clk <= 0) return 0;
	if (clk > CLK_MAX) clk = CLK_MAX;
	return (int)clk;
}

char *print_speed(int clk) {
	static char buf[16];
	char m = '\0';
	double c = clk;
	if (c >= 1e6) {
		c /= 1e6;
		m = 'M';
	} else if (c >= 1e3) {
		c /= 1e3;
		m = 'K';
	}
	sprintf(buf, "%g%c", c, m);
	return buf;
}

static int get_speed() {
	int clk = CLK_DIV5;
	if (div5[0] == MP_DIV5DI) {
		clk = CLK_RAW;
	}
	int div = setclk[1];
	div |= (setclk[2] << 8);
	return (clk / ((1 + div) * 2));
}

// Must be called before open.
// Setup clock speed 'hz'.
// Returns actual clock speed setup.
int spi_speed(int hz) {
	if (hz <= 0) {
		return get_speed();
	}
	if (hz > CLK_MAX) {
		hz = CLK_MAX;
	}
	int clk;
	if (hz > CLK_DMAX) {
		div5[0] = MP_DIV5DI;
		clk = CLK_RAW;
	} else {
		div5[0] = MP_DIV5EN;
		clk = CLK_DIV5;
	}
	int div = (clk / 2 / hz) - 1;
	if (div < 0) div = 0;
	if (div > 0xffff) div = 0xffff;
	setclk[1] = div & 0xff;
	setclk[2] = (div >> 8) & 0xff;
	return get_speed();
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
	unsigned char flsh[] = { MP_FLUSH };
	int n = spi_write(ftHandle, flsh, sizeof(flsh));
	if (n < 0 || n != sizeof(flsh)) {
		return;
	}
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
		dump_buf(buf, 0xf800, bytesRead);
	}
	free(buf);
}

// Set /CS on (low) or off (high).
int spi_cs(FT_HANDLE ftHandle, int on) {
	unsigned char setio[] = {
		MP_SETIO, IOINIT, IODIR
	};
	if (on) {
		setio[1] &= ~IO_CS; // clear bit = on, active low signal
	} else {
		setio[1] |= IO_CS; // set bit = off, active low signal
	}
	int n = spi_write(ftHandle, setio, sizeof(setio));
	if (n < 0 || n != sizeof(setio)) {
		return -1;
	}
	return 0;
}

int spi_setup(FT_HANDLE ftHandle) {
	int n;
	unsigned char setup[] = {
		MP_SETIO, IOINIT, IODIR
	};
	unsigned char loopback[] = {
		MP_NOLOOP	// loopback off
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
	n = spi_write(ftHandle, div5, sizeof(div5));
	if (n < 0 || n != sizeof(div5)) {
		return -1;
	}
	n = spi_write(ftHandle, setclk, sizeof(setclk));
	if (n < 0 || n != sizeof(setclk)) {
		return -1;
	}
	return 0;
}

// Send SPI write prefix - prepare to send data to SPI device
// (not commands to C232HM).
static int spi_prep(FT_HANDLE ftHandle, DWORD len) {
	unsigned char xfer[] = {
		MP_CLKBYTES, 0x00, 0x00  // Write + read; length bytes set below.
	};
	len -= 1;	// field is length-1...
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
	//spi_flush(ftHandle);
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
	//spi_flush(ftHandle);
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
