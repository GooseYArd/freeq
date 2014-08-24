#include "byte.h"
#include "buffer.h"
#include <stdint.h>

/* This code was shamelessly stolen and adapted from beautiful C-only
   version of protobuf by 云风 (cloudwu) , available at:
   https://github.com/cloudwu/pbc */

inline int
buffer_putvarint32(buffer *b, uint32_t number)
{
	if (number < 0x80) {
		//buffer[0] = (uint8_t) number; 
                buffer_putbyte(b, number);
		return 1;
	}
	//buffer[0] = (uint8_t) (number | 0x80 );
        buffer_putbyte(b, (number | 0x80));
	if (number < 0x4000) {
		//buffer[1] = (uint8_t) (number >> 7 );
                buffer_putbyte(b, (number >> 7 ));
		return 2;
	}
	//buffer[1] = (uint8_t) ((number >> 7) | 0x80 );
        buffer_putbyte(b, ((number >> 7) | 0x80 ));
	if (number < 0x200000) {
		//buffer[2] = (uint8_t) (number >> 14);
                buffer_putbyte(b, (number >> 14));
		return 3;
	}
	//buffer[2] = (uint8_t) ((number >> 14) | 0x80 );
        buffer_putbyte(b, ((number >> 14) | 0x80 ));
	if (number < 0x10000000) {
		//buffer[3] = (uint8_t) (number >> 21);
                buffer_putbyte(b, (number >> 21));
		return 4;
	}
	//buffer[3] = (uint8_t) ((number >> 21) | 0x80 );
	//buffer[4] = (uint8_t) (number >> 28);
        buffer_putbyte(b, ((number >> 21) | 0x80 ));
        buffer_putbyte(b, (number >> 28));
	return 5;
}

int
buffer_putvarint(buffer *b, uint64_t number) 
{
	if ((number & 0xffffffff) == number) {
		/* return _pbcV_encode32((uint32_t)number , buffer); */
		return buffer_putvarint32(b, (uint32_t)number);
	}
	int i = 0;
	do {
		//buffer[i] = (uint8_t)(number | 0x80);
                buffer_putbyte(b, (uint8_t)(number | 0x80));
		number >>= 7;
		++i;
	} while (number >= 0x80);
	//buffer[i] = (uint8_t)number;
        buffer_putbyte(b, (uint8_t)number);
	return i+1;
}

int 
buffer_putvarintsigned32(buffer *b, int32_t n)
{
	n = (n << 1) ^ (n >> 31);
	return buffer_putvarint32(b,n);
}

int 
buffer_putvarintsigned(buffer *b, int64_t n)
{
	n = (n << 1) ^ (n >> 63);
	return buffer_putvarint(b,n);
}
