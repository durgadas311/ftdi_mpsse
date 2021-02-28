#ifndef __SPILIB_H__
#define __SPILIB_H__

#include "ftd2xx.h"

void dump_buf(unsigned char *buf, int len);

// For now, this serves as 'errno'
extern FT_STATUS ftStatus;
extern DWORD driverVersion;

// Normally, only open, close, and xfer are used, but provide access anyway...
int spi_write(FT_HANDLE ftHandle, unsigned char *buf, const int len);
int spi_setup(FT_HANDLE ftHandle, DWORD len);
int spi_read(FT_HANDLE ftHandle, unsigned char *buf, int len);
int spi_begin(FT_HANDLE ftHandle, int len);
int spi_end(FT_HANDLE ftHandle);

// Returns bytes read, or -1 on error.
int spi_xfer(FT_HANDLE ftHandle, unsigned char *bufout,
			unsigned char *bufin, const int len);
FT_HANDLE spi_open(int port);
void spi_close(FT_HANDLE ftHandle);

#endif /* __SPILIB_H__ */
