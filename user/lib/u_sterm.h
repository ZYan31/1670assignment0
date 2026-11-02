#ifndef _U_STERM_H
#define _U_STERM_H

#include "u_types.h"

// A text console on the serial line: the serial-console counterpart of the
// u_vterm library you wrote for your graphical terminal in Project 3.
//
// Here, the drawing is done by the terminal program on your computer (the one
// you run `make qemu` in): this library just sends it bytes, using write() on
// file descriptor 1, and uses ANSI escape sequences for the few things that
// are not plain text (clearing the screen, moving the cursor home). As with
// u_vterm, it is output only: reading the keyboard, editing a line, and
// deciding what a line means are the program's business.
//
// It needs exactly one thing from you: a kernel that implements the write()
// system call from the Syscall ABI (system call 2) for the console. It makes
// that system call itself, so it does not need your write() stub, and it
// links into every program even before you have one.

struct sterm {
  int fd;  // where output goes: descriptor 1 (stdout) after sterm_init()
};

// Set up `t` to write to standard output (descriptor 1). Does not clear the
// screen; call sterm_clear() if you want that.
void sterm_init(struct sterm* t);

// Clear the whole screen and put the cursor at the top left.
void sterm_clear(struct sterm* t);

// Write one character. '\n' moves to the start of the next line (it is sent
// as "\r\n"); everything else, including '\r', '\b' and '\t', is sent as is
// and interpreted by your computer's terminal. To erase the character before
// the cursor, send "\b \b" (back up, overwrite with a space, back up).
void sterm_putc(struct sterm* t, char ch);

// Write a NUL-terminated string, with the same treatment of '\n'.
void sterm_puts(struct sterm* t, const char* s);

// Format like printf() and write the result. Supports %c, %s, %d, %i, %u,
// %x, %X, %p and %%, the length modifiers l and ll, a minimum field width,
// and the flags '-' (left-align) and '0' (pad with zeros). Output longer than
// STERM_PRINTF_MAX bytes is cut off.
#define STERM_PRINTF_MAX 256
void sterm_printf(struct sterm* t, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif  // _U_STERM_H
