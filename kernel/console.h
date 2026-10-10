#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "ringbuf.h"
#include "types.h"

extern ringbuf_t input_buf;             // defined in console.c
void console_init(void);
void console_handle_input(char c);

#endif  // _CONSOLE_H
