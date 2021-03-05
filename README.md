# ftdi_mpsse
## Workspace for utilities to run on the FTDI MPSSE devices

The C232HM-EDHSL and C232HM-DDHSL devices from FTDI provide
a Multi-Protocol Synchronous Serial Engine (MPSSE)
that is capable of running in an SPI mode at speeds up to 30MHz,
with 64K-byte bursts of data.
The MPSSE is connected to the host via USB.
SPI devices may be interfaced to the MPSSE and accessed directly from the host.
This allows exploration and debug of the SPI device,
and even provides a simple programming capability
for SPI-connected flash-memory devices.

http://www.ftdichip.com/Support/Documents/DataSheets/Cables/DS_C232HM_MPSSE_CABLE.pdf

Note, Application Note 108 (AN-108), referenced in the datasheet, has a wealth
of information on the MPSSE commands to setup the interface to the target device.

An alternative, and in some ways superior, device is described at https://spidriver.com/.
That device bit-bangs the SPI and is fixed at about a 500KHz bit rate, but
has a nice "logic analyzer" display built-in.
It is also limited to 64-byte bursts of data.

The MPSSE device requires root privileges and is also incompatible
with the usbserial driver (which must be removed). It should be possible to
setup the system so that root privileges are not required, and also so that
other usbserial devices can remain active. TBD.

Interfacing to SPI devices typically requires more than simple connection,
as (at least) pull-up resistors are often required on certain pins.
It may be beneficial to build a breadboard interface, such as
![Breadboard Example](breadboard-1.jpg)
