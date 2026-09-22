#ifndef _PRINTF_H
#define _PRINTF_H

#include "types.h"

// Formatted output over the UART. Supported conversions:
//   %d %u        (int / unsigned int, base 10)
//   %ld %lu      (long / unsigned long, base 10)
//   %x %lx       (hex; %lx for long)
//   %p           (pointer, printed as 0x...)
//   %c %s %%     (char, string, literal percent)
// A leading zero and width (e.g. %08x) zero-pads numeric conversions;
// a bare width (e.g. %8x) space-pads. An unsupported specifier or a bad
// argument (e.g. a null %s) emits a bracketed error message instead.
void printf(const char* fmt, ...);

// Same as printf, but takes an already-started va_list.
void vprintf(const char* fmt, va_list args);

// Print a message like printf, then halt the machine forever (never returns).
void panic(const char* fmt, ...);

#endif  // _PRINTF_H
