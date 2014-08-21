#include "byte.h"
#include "buffer.h"
#include <stdint.h>

ssize_t buffer_getvarint(buffer *b, struct longlong *result) {
        char x;
        
        if (!buffer_getc(b, &x))
                return 0;

	if (!(x & 0x80)) {
		result->low = x;
		result->hi = 0;
		return 1;
	}
	uint32_t r = x & 0x7f;
	int i;
	for (i=1;i<4;i++) {
                buffer_getc(b, &x);
		r |= ((x&0x7f) << (7*i));
		if (!(x & 0x80)) {
			result->low = r;
			result->hi = 0;
			return i+1;
		}
	}
	uint64_t lr = 0;
	for (i=4;i<10;i++) {
                buffer_getc(b, &x);
		lr |= ((uint64_t)(x & 0x7f) << (7*(i-4)));
		if (!(x & 0x80)) {
			result->hi = (uint32_t)(lr >> 4);
			result->low = r | (((uint32_t)lr & 0xf) << 28);
			return i+1;
		}
	}

	result->low = 0;
	result->hi = 0;
	return 10;        
}
