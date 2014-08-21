#include "byte.h"
#include "buffer.h"

extern int buffer_stubborn(ssize_t (*op)(),int fd,const char* buf, size_t len,void* cookie);

int buffer_putbyte(buffer* b, char val) {
        int len = 1;
  if (len > b->a-b->p) {	/* doesn't fit */
    if (buffer_flush(b)==-1) return -1;
    if (len>b->a) {
            if (buffer_stubborn(b->op,b->fd,(const char *)&val,1,b)<0) return -1;
      return 0;
    }
  }
  byte_copy(b->x+b->p, 1, &val);
  b->p+=len;
  return 0;
}
