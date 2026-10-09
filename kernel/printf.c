#include "printf.h"

#include "drivers/uart.h"
#include "types.h"
#include "interrupts.h"   // disable_interrupts / restore_interrupts (Quest 4 Part E)

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
static void send_padded(const char* buf, int len, int width) {
  for (int i = len; i < width; i++) {
    uart_send('0');
  }
  uart_send_string(buf);
}

static void vprintf_inner(const char* fmt, va_list args) {
  char buf[32];

  while (*fmt != '\0') {
    char c = *fmt++;
    if (c != '%') {
      uart_send(c);
      continue;
    }

    // Extra credit: '%!' introduces a DECwriter LA12 control code. Each case
    // just pushes a DEC/ANSI escape sequence out the UART (ESC = 0x1B).
    // Comment out this block to disable the extra-credit feature.
    if (*fmt == '!') {
      fmt++;
      char attr = *fmt++;
      switch (attr) {
        // All sequences below are real LA100/LA12 codes (EK-LA100-RM-001).
        case 'u': uart_send_string("\x1b[4m"); break;  // underline on  (S2.4.1.6)
        case 'r': uart_send_string("\x1b[0m"); break;  // clear underline (S2.4.1.6)
        case 'p': {                                     // horizontal pitch (S2.4.1.9)
          char d = *fmt;                                // ESC [ <digit> w
          if (d >= '0' && d <= '9') {                   // 1=10cpi 2=12 4=16.5 5=5cpi
            fmt++;
            uart_send('\x1b');
            uart_send('[');
            uart_send(d);
            uart_send('w');
          } else {
            uart_send_string("[printf: bad pitch]");
          }
          break;
        }
        case 'v': {                                     // vertical pitch (S2.4.1.12)
          char d = *fmt;                                // ESC [ <digit> z
          if (d >= '0' && d <= '9') {                   // 0/1=6lpi 2=8 3=12 4=2lpi
            fmt++;
            uart_send('\x1b');
            uart_send('[');
            uart_send(d);
            uart_send('z');
          } else {
            uart_send_string("[printf: bad vpitch]");
          }
          break;
        }
        case 'f': {                                     // font select (S2.4.1.6)
          char d = *fmt;                                // ESC [ 1<digit> m
          if (d >= '0' && d <= '9') {                   // f0=Font1 ... f4=Font5
            fmt++;
            uart_send('\x1b');
            uart_send('[');
            uart_send('1');
            uart_send(d);
            uart_send('m');
          } else {
            uart_send_string("[printf: bad font]");
          }
          break;
        }
        case '\0': //nul terminator.
          uart_send_string("[printf: invalid format specifier]");
          return;
        default: //fits no other cases
          uart_send_string("[printf: invalid format specifier]");
          break;
      }
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
        if (neg) uart_send('-');
        for (int i = total; i < width; i++) uart_send('0');  // pad with zeroes
        uart_send_string(buf);  // send the digits
        break;
      }
      case 'u': {
        uint64 v = is_long ? va_arg(args, uint64) : (uint64)va_arg(args, uint32);
        send_padded(buf, u64_to_str(v, 10, buf), width);
        break;
      }
      case 'x': {
        uint64 v = is_long ? va_arg(args, uint64) : (uint64)va_arg(args, uint32);
        send_padded(buf, u64_to_str(v, 16, buf), width);
        break;
      }
      case 'p': {
        uint64 v = (uint64)(uintptr_t)va_arg(args, void*);
        uart_send('0');
        uart_send('x');
        send_padded(buf, u64_to_str(v, 16, buf), width);
        break;
      }
      case 'c': {
        // A char argument is promoted to int when passed through '...'.
        uart_send((char)va_arg(args, int32));
        break;
      }
      case 's': {
        const char* s = va_arg(args, char*);
        uart_send_string(s != 0 ? s : "[printf: null string argument]");
        break;
      }
      case '%': {
        uart_send('%');
        break;
      }
      case '\0': {
        // Format string ended right after a '%'.
        uart_send_string("[printf: invalid format specifier]");
        return;
      }
      default: {
        uart_send_string("[printf: invalid format specifier]");
        break;
      }
    }
  }
}

// Part E: emit a whole message without being preempted mid-line. Mask IRQs
// around the output path and restore the caller's previous state afterward.
void vprintf(const char* fmt, va_list args) {
  unsigned long flags = disable_interrupts();
  vprintf_inner(fmt, args);
  restore_interrupts(flags);
}

void printf(const char* fmt, ...) {
  va_list args; //
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

void panic(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  for (;;) {
  }  // halt: no OS to return to
}
