## spilib

### These are the interfaces availabe in spilib.h (spilib.c):

**`int spi_speed(int hz)`**
-   Setup SPI clock speed to nearest value.
-   Must be called before spi_open() to take effect.

**`FT_HANDLE spi_open(int port)`**
-   Open C232HM at port number specified.
-   Sets up device for:
    -   Timeout 3 seconds.
    -   Latency Timer 2mS.
    -   TDO as input, TMS, TCK, TDI, GPIOL0-3 as outputs.
    -   TMS, GPIOL0-3 are logic "1" (high), TCK, TDI are "0".
    -   SPI clock speed set.
    -   Driver version retrieved.
-   Returns NULL on error.

**`void spi_close(FT_HANDLE ftHandle)`**
-   Invalidates handle and resets C232HM.

**`int spi_xfer(FT_HANDLE ftHandle, unsigned char *bufout,
			unsigned char *bufin, const int len)`**
-   Send bufout data to SPI device, save received data in bufin.
-   Performs chip-select as speicfied by last set_cs(), default is TMS.
-   Limited to 64 bytes total.

**`int spi_xfer_long(FT_HANDLE ftHandle, unsigned char *bufout,
			unsigned char *bufin, const int len)`**
-   Send bufout data to SPI device, save received data in bufin.
-   Performs chip-select as speicfied by last set_cs(), default is TMS.
-   Handles transfers longer than 64 bytes.

**`int set_cs(char cs)`**
-   Choose CS gpio bit, '0'..'3','C' for GPIOL0-3,TMS

**`void dump_buf(unsigned char *buf, int off, int len)`**
-   Convenience routine to dump data.
-   'off' is added to index when printing addresses.

**`void dump_buf2(unsigned char *buf, int bsb, int off, int len)`**
-   Convenience routine to dump data with two-part addresses.
-   'bsb' is prefixed to each line.
-   'off' is added to index when printing addresses.

**`int parse_speed(char *arg)`**
-   Parse commandline argument for speed.
-   For example, "1.2M" is for 1.2MHz.

**`char *print_speed(int clk)`**
-   Reverse of parse_speed(), to pretty-print speed value.
-   Does not include "Hz".

**`FT_STATUS ftStatus`**
-   Global variable for stastus from last FT_Xxxx() routine called.

**`DWORD driverVersion`**
-   Global variable containing driver version, after spi_open().

### The following are not normally used external to spilib.c:

**`int spi_write(FT_HANDLE ftHandle, unsigned char *buf, const int len)`**
-   Low-level write to C232HM.

**`int spi_setup(FT_HANDLE ftHandle, DWORD len)`**
-   Called by spi_open() to complete setup of device.

**`int spi_read(FT_HANDLE ftHandle, unsigned char *buf, int len)`**
-   Wait for 'len' bytes to accumulate and transfer to 'buf'.

**`int spi_begin(FT_HANDLE ftHandle, int len)`**
-   Activates chip select.
-   Prepare SPI for a transfer of length 'len'.
    -   Sends command prefix to do SPI write/read, data must follow.

**`int spi_end(FT_HANDLE ftHandle)`**
-   Finishes SPI transfer.
-   Deactivates chip select.

**`int spi_cs(FT_HANDLE ftHandle, int on)`**
-   Sets currently chosen chip select to on (1) or off (0).
