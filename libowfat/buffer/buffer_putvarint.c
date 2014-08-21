#include "byte.h"
#include "buffer.h"
#include <stdint.h>

inline int
buffer_putvarint32(uint32_t number, buffer *b)
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

