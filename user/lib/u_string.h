#ifndef _U_STRING_H
#define _U_STRING_H

#include "u_types.h"

// Formatted output over the UART. Supported conversions:
//   %d %u        (int / unsigned int, base 10)
//   %ld %lu      (long / unsigned long, base 10)
//   %x %lx       (hex; %lx for long)
//   %p           (pointer, printed as 0x...)
//   %c %s %%     (char, string, literal percent)
// A leading zero and width (e.g. %08x) zero-pads numeric conversions;
// a bare width (e.g. %8x) space-pads. An unsupported specifier or a bad
// argument (e.g. a null %s) emits a bracketed error message instead.
void sprintf(char* out, const char* fmt, ...);

// Same as printf, but takes an already-started va_list.
void vsprintf(char* out, const char* fmt, va_list args);

// Bounded vsprintf: writes at most `size` bytes to `out` (NUL included) and
// returns the number of bytes written (excluding the NUL). Safe for printf.
int vsnprintf(char* out, uint64 size, const char* fmt, va_list args);

// Compare two strings: 0 if equal (like the C library's strcmp).
int strcmp(const char* a, const char* b);

// Length of a NUL-terminated string, not counting the '\0'.
uint32 strlen(const char* s);

// Copy n bytes from src to dest (non-overlapping), a machine word at a time.
void* memcpy(void* dest, const void* src, uint64 n);
#endif
