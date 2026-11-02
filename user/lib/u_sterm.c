#include "u_sterm.h"

#include "u_types.h"

// The write() system call, as the Syscall ABI defines it: `svc #2`, with the
// arguments in x0-x2 and the result in x0. This library makes the system call
// itself, with its own copy of the two-instruction stub, rather than calling
// your write() stub: everything in user/lib/ is linked into every program, so
// a call to your stub would stop every program from linking until you have
// one. Like the snake program, this file depends only on the ABI. (Until your
// kernel implements system call 2, it returns -1, and nothing is printed.)
int64 sterm_sys_write(int fd, const void* buf, uint64 n);
asm(".pushsection .text\n"
    "  .global sterm_sys_write\n"
    "  .type sterm_sys_write, %function\n"
    "sterm_sys_write:\n"
    "  svc #2\n"
    "  ret\n"
    "  .size sterm_sys_write, . - sterm_sys_write\n"
    ".popsection\n");

// ANSI escape sequences: ESC [ 2 J clears the screen, ESC [ H moves the cursor
// to the top left.
#define ANSI_CLEAR_SCREEN "\x1b[2J"
#define ANSI_CURSOR_HOME "\x1b[H"

static uint64 str_len(const char* s) {
  uint64 n = 0;
  while (s[n] != '\0') {
    n++;
  }
  return n;
}

// Write all `n` bytes of `buf`, retrying after partial writes. Gives up
// quietly on an error: there is nowhere else to report it.
static void write_all(int fd, const char* buf, uint64 n) {
  while (n > 0) {
    int64 written = sterm_sys_write(fd, buf, n);
    if (written <= 0) {
      return;
    }
    buf += written;
    n -= (uint64)written;
  }
}

void sterm_init(struct sterm* t) {
  t->fd = 1;
}

void sterm_clear(struct sterm* t) {
  write_all(t->fd, ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME,
            str_len(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME));
}

void sterm_putc(struct sterm* t, char ch) {
  if (ch == '\n') {
    write_all(t->fd, "\r\n", 2);
  } else {
    write_all(t->fd, &ch, 1);
  }
}

// Writes `s` in as few write() calls as possible: one per run of characters
// between newlines, plus one per newline.
void sterm_puts(struct sterm* t, const char* s) {
  while (*s != '\0') {
    uint64 run = 0;
    while (s[run] != '\0' && s[run] != '\n') {
      run++;
    }
    write_all(t->fd, s, run);
    s += run;
    if (*s == '\n') {
      write_all(t->fd, "\r\n", 2);
      s++;
    }
  }
}

// ---------------------------------------------------------------------------
// A small, bounded formatter for sterm_printf(). It never writes more than
// `size` bytes into `buf` (including the terminating NUL).
// ---------------------------------------------------------------------------

struct fmt_out {
  char* buf;
  uint64 size;  // capacity of buf, including space for the NUL
  uint64 len;   // characters written so far (excluding the NUL)
};

static void out_char(struct fmt_out* o, char c) {
  if (o->len + 1 < o->size) {
    o->buf[o->len] = c;
  }
  o->len++;
}

static void out_repeat(struct fmt_out* o, char c, int count) {
  for (int i = 0; i < count; i++) {
    out_char(o, c);
  }
}

// Emit `digits` (already in the right order) with an optional sign, padded
// to `width` as the flags say.
static void out_padded(struct fmt_out* o, const char* digits, int ndigits,
                       bool negative, int width, bool left, bool zero) {
  int total = ndigits + (negative ? 1 : 0);
  int pad = width > total ? width - total : 0;

  if (!left && !zero) {
    out_repeat(o, ' ', pad);
  }
  if (negative) {
    out_char(o, '-');
  }
  if (!left && zero) {
    out_repeat(o, '0', pad);
  }
  for (int i = 0; i < ndigits; i++) {
    out_char(o, digits[i]);
  }
  if (left) {
    out_repeat(o, ' ', pad);
  }
}

static void out_unsigned(struct fmt_out* o, uint64 value, unsigned base,
                         bool upper, bool negative, int width, bool left,
                         bool zero) {
  const char* digit_chars = upper ? "0123456789ABCDEF" : "0123456789abcdef";
  char reversed[24];
  char digits[24];
  int n = 0;

  do {
    reversed[n++] = digit_chars[value % base];
    value /= base;
  } while (value != 0);

  for (int i = 0; i < n; i++) {
    digits[i] = reversed[n - 1 - i];
  }
  out_padded(o, digits, n, negative, width, left, zero);
}

static uint64 sterm_vsnprintf(char* buf, uint64 size, const char* fmt,
                              va_list ap) {
  struct fmt_out o = {.buf = buf, .size = size, .len = 0};

  for (const char* p = fmt; *p != '\0'; p++) {
    if (*p != '%') {
      out_char(&o, *p);
      continue;
    }
    p++;

    // Flags
    bool left = false, zero = false;
    for (;; p++) {
      if (*p == '-') {
        left = true;
      } else if (*p == '0') {
        zero = true;
      } else {
        break;
      }
    }

    // Minimum field width
    int width = 0;
    while (*p >= '0' && *p <= '9') {
      width = width * 10 + (*p - '0');
      p++;
    }

    // Length modifier: l or ll (both mean 64 bits here)
    bool is_long = false;
    while (*p == 'l') {
      is_long = true;
      p++;
    }

    switch (*p) {
      case 'c': {
        char c = (char)va_arg(ap, int);
        out_padded(&o, &c, 1, false, width, left, false);
        break;
      }
      case 's': {
        const char* s = va_arg(ap, const char*);
        if (s == 0) {
          s = "(null)";
        }
        out_padded(&o, s, (int)str_len(s), false, width, left, false);
        break;
      }
      case 'd':
      case 'i': {
        int64 v = is_long ? va_arg(ap, int64) : (int64)va_arg(ap, int);
        bool negative = v < 0;
        uint64 magnitude = negative ? (uint64)0 - (uint64)v : (uint64)v;
        out_unsigned(&o, magnitude, 10, false, negative, width, left, zero);
        break;
      }
      case 'u': {
        uint64 v = is_long ? va_arg(ap, uint64) : (uint64)va_arg(ap, uint32);
        out_unsigned(&o, v, 10, false, false, width, left, zero);
        break;
      }
      case 'x':
      case 'X': {
        uint64 v = is_long ? va_arg(ap, uint64) : (uint64)va_arg(ap, uint32);
        out_unsigned(&o, v, 16, *p == 'X', false, width, left, zero);
        break;
      }
      case 'p': {
        uint64 v = (uint64)va_arg(ap, void*);
        out_char(&o, '0');
        out_char(&o, 'x');
        out_unsigned(&o, v, 16, false, false, width > 2 ? width - 2 : 0,
                     left, zero);
        break;
      }
      case '%':
        out_char(&o, '%');
        break;
      case '\0':
        // A lone '%' at the end of the format string: print it and stop.
        out_char(&o, '%');
        p--;
        break;
      default:
        // Unknown conversion: print it unchanged.
        out_char(&o, '%');
        out_char(&o, *p);
        break;
    }
  }

  if (o.size > 0) {
    o.buf[o.len < o.size ? o.len : o.size - 1] = '\0';
  }
  return o.len;
}

void sterm_printf(struct sterm* t, const char* fmt, ...) {
  char buf[STERM_PRINTF_MAX];

  va_list ap;
  va_start(ap, fmt);
  sterm_vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  sterm_puts(t, buf);
}
