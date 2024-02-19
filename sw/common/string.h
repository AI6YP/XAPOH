#ifndef	_STRING_H
#define	_STRING_H

#include <stddef.h>

void* memcpy(void *dest, const void *src, size_t len) {
  char *csrc = (char *)src;
  char *cdest = (char *)dest;
  for (unsigned int i = 0; i < len; i++) {
    cdest[i] = csrc[i];
  }
  return dest;
}

void* memset(void *dest, int ch, size_t count) {
  unsigned char* p = dest;
  while(count--) { *p++ = (unsigned char)ch; }
  return dest;
}

#endif
