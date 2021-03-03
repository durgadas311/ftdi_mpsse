/**************************************************************************
//
// crc16.c - generate a ccitt 16 bit cyclic redundancy check (crc)
//
//      The code in this module generates the crc for a block of data.
//
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
//                                16  12  5
// The CCITT CRC 16 polynomial is X + X + X + 1.
// In binary, this is the bit pattern 1 0001 0000 0010 0001, and in hex it
//  is 0x11021.
// A 17 bit register is simulated by testing the MSB before shifting
//  the data, which affords us the luxury of specifiy the polynomial as a
//  16 bit value, 0x1021.
// Due to the way in which we process the CRC, the bits of the polynomial
//  are stored in reverse order. This makes the polynomial 0x8408.
*/
#define POLY 0x8408

static unsigned short crc;

static void crc_byte(unsigned char data) {
	int i;
	int mask;

	for (i = 0; i < 8; i++) {
		mask = ((crc ^ data) & 1);
		crc >>= 1;
		data >>= 1;
		if (mask) {
			crc ^= POLY;
		}
	}
}

unsigned short crc16(unsigned char *data_p, int length) {
	crc = 0xffff;
	if (length == 0)
		return (~crc);
	do {
		unsigned char b = *data_p++;
		crc_byte(b);
//printf("%02x => %04x\n", b, crc);
	} while (--length);
	return crc;
}
