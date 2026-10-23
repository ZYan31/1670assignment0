#include "u_string.h"

// Quest 5 B: word-at-a-time copy. The console scroll moves most of a canvas for
// every new line, so copying 8 bytes per step instead of 1 matters on the Pi.
// Source and destination never overlap here, so we always copy forward. Both
// pointers must be the same distance from an 8-byte boundary or we cannot align
// both; with the MMU off an unaligned 64-bit access faults, so fall back to a
// byte copy in that case.
void* memcpy(void* dest, const void* src, uint64 n) {
  uint8* d = (uint8*)dest;
  const uint8* s = (const uint8*)src;
  if (((uint64)d & 7u) != ((uint64)s & 7u)) {
    while (n) { *d++ = *s++; n--; }
    return dest;
  }
  while (n && ((uint64)d & 7u)) { *d++ = *s++; n--; }   // align to an 8-byte boundary
  uint64* dw = (uint64*)d;
  const uint64* sw = (const uint64*)s;
  while (n >= 8) { *dw++ = *sw++; n -= 8; }              // a machine word at a time
  d = (uint8*)dw; s = (const uint8*)sw;
  while (n) { *d++ = *s++; n--; }                        // the trailing bytes
  return dest;
}

// Convert an unsigned 64-bit value to a string in `base` (10 or 16), writing a
// NUL-terminated result into `buf` and returning its length (excluding NUL).
static int u64_to_str(uint64 value, uint32 base, char* buf) {
  const char* digits = "0123456789abcdef";
  char tmp[32];
  int i = 0;

  // Build the digits by repeatedly dividing and taking the remainder.
  if (value == 0) {
    tmp[i++] = '0';
  } else {
    while (value != 0) {
      tmp[i++] = digits[value % base];
      value /= base;
    }
  }

  // The digits came out least-significant first; reverse into `buf`.
  int len = i;
  for (int j = 0; j < len; j++) {
    buf[j] = tmp[len - 1 - j];
  }
  buf[len] = '\0';
  return len;
}

// Print `buf`, first adding enough leading '0's to reach `width` columns.
static void send_padded(char** p, const char* buf, int len, int width) {
  for (int i = len; i < width; i++) {
    *(*p)++ = '0';
  }
  for (const char* q = buf; *q; ) *(*p)++ = *q++;
}

void vsprintf(char* out, const char* fmt, va_list args) {
  char buf[32];
  char* p = out;
  while (*fmt != '\0') {
    char c = *fmt++;
    if (c != '%') {
      *p++ = c;
      continue;
    }

    // Optional leading-zero width, e.g. "%08x".
    int width = 0;
    if (*fmt == '0') {
      fmt++;
      while (*fmt >= '0' && *fmt <= '9') {
        width = width * 10 + (*fmt - '0');
        fmt++;
      }
    }

    // Optional 'l' length modifier (argument is a long / 64-bit).
    int is_long = 0;
    if (*fmt == 'l') {
      is_long = 1;
      fmt++;
    }

    char spec = *fmt++;
    switch (spec) {
      case 'd': {
        int64 v = is_long ? va_arg(args, int64) : (int64)va_arg(args, int32);
        // if long, assign 64 bits, if not, assign 32.
        int neg = v < 0;  // checking the signedness of integers/specs
        uint64 mag = neg ? (uint64)(-v) : (uint64)v;  // abs. value
        int len = u64_to_str(mag, 10, buf);  // convert to base 10 into buffer.
        int total = len + (neg ? 1 : 0);  // digits plus a possible '-'
        if (neg) *p++ = '-';
        for (int i = total; i < width; i++) *p++ = '0';  // pad with zeroes
        for (const char* q = buf; *q; ) *p++ = *q++;  // send the digits
        break;
      }
      case 'u': {
        uint64 v = is_long ? va_arg(args, uint64) : (uint64)va_arg(args, uint32);
        send_padded(&p, buf, u64_to_str(v, 10, buf), width);
        break;
      }
      case 'x': {
        uint64 v = is_long ? va_arg(args, uint64) : (uint64)va_arg(args, uint32);
        send_padded(&p, buf, u64_to_str(v, 16, buf), width);
        break;
      }
      case 'p': {
        uint64 v = (uint64)(uintptr_t)va_arg(args, void*);
        *p++ = '0';
        *p++ = 'x';
        send_padded(&p, buf, u64_to_str(v, 16, buf), width);
        break;
      }
      case 'c': {
        // A char argument is promoted to int when passed through '...'.
        *p++ = (char)va_arg(args, int32);
        break;
      }
      case 's': {
        const char* s = va_arg(args, char*);
        for (const char* q = (s != 0 ? s : "[sprintf: null string argument]"); *q; ) *p++ = *q++;
        break;
      }
      case '%': {
        *p++ = '%';
        break;
      }
      case '\0': {
        // Format string ended right after a '%'.
        for (const char* q = "[sprintf: invalid format specifier]"; *q; ) *p++ = *q++;
        return;
      }
      default: {
        for (const char* q = "[sprintf: invalid format specifier]"; *q; ) *p++ = *q++;
        break;
      }
    }
  }
  *p = '\0';
}

// Part E: emit a whole message without being preempted mid-line. Mask IRQs
// around the output path and restore the caller's previous state afterward.

void sprintf(char* out, const char* fmt, ...) {
  va_list args; //
  va_start(args, fmt);
  vsprintf(out, fmt, args);
  va_end(args);
}

// Compare two strings: 0 if equal, else the signed difference of the first
// mismatching byte (like the C library's strcmp).
int strcmp(const char* a, const char* b) {
  while (*a && *a == *b) { a++; b++; }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

// Length of a NUL-terminated string, not counting the '\0'.
uint32 strlen(const char* s) {
  uint32 n = 0;
  while (*s++) n++;
  return n;
}
