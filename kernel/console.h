#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "ringbuf.h"
#include "types.h"

extern int32 foreground_pid;
void console_init(void);
void console_handle_input(char c);
char console_getc(void);
int32 console_hasc(void);

#endif  // _CONSOLE_H
