#include "utils.h"

#include "printf.h"

unsigned int delay_cycles(uint64 ctr) {
  while (ctr--) {
    __asm__ volatile("nop");
  }
  return ctr;
}

void* memmove(void* dest, const void* src, uint64 n) {
  // We use 8-bit (1 byte) pointers to copy byte-by-byte.
  uint8* d = dest;
  const uint8* s = src;

  if (d == s || n == 0) {
    return dest;
  }

  if (d < s) {  // Copy forwards if destination before source
    for (uint64 i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    // Source before destination, copy backwards to handle overlap
    for (uint64 i = n; i != 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }

  return dest;
}

void* memcpy(void* dest, const void* src, uint64 n) {
  // return memmove(dest, src, n);   // Quest 5 B: memcpy now has its own word-at-a-time body
  uint8* d = (uint8*)dest;
  const uint8* s = (const uint8*)src;

  // memcpy's source and destination never overlap, so we always copy forward.
  // Both pointers must be the same distance from an 8-byte boundary or we can
  // never align both; with the MMU off an unaligned 64-bit access is not slow,
  // it faults (ESR_EL1 class 0x25), so fall back to a byte copy in that case.
  if (((uint64)d & 7u) != ((uint64)s & 7u)) {
    while (n) { *d++ = *s++; n--; }
    return dest;
  }
  // 1) copy bytes until the destination (and hence src) is 8-byte aligned
  while (n && ((uint64)d & 7u)) { *d++ = *s++; n--; }
  // 2) copy a machine word (8 bytes) at a time while at least 8 remain
  uint64* dw = (uint64*)d;
  const uint64* sw = (const uint64*)s;
  while (n >= 8) { *dw++ = *sw++; n -= 8; }
  // 3) copy the remaining trailing bytes
  d = (uint8*)dw; s = (const uint8*)sw;
  while (n) { *d++ = *s++; n--; }
  return dest;
}

void* memset(void* src, int c, uint64 n) {
  char* s = src;

  for (uint64 i = 0; i < n; ++i) {
    s[i] = c;
  }

  return src;
}
