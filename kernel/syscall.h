#ifndef SYSCALL_H
#define SYSCALL_H
#define SYS_yield 0
#define SYS_ticks 1
#define SYS_write 2
#define SYS_read 3
#define SYS_peek 4
#define SYS_getpid 5
#define SYS_process_info 6
#define SYS_init_canvas 7
#define SYS_wait_for_vsync 8
// Reserved for later syscalls. Each SYS_* line auto-generates a user stub of
// the same name, so these stay commented until implemented -- and note
// exit already exists as a library function in u_common.c, so that name
// will collide unless the stub replaces the C function.
// #define SYS_spawn 9
// #define SYS_exit 10
// #define SYS_wait 11
// #define SYS_pipe 12
// #define SYS_close 13
// #define SYS_open_console 51
#include "types.h"
#include "context.h"
[[noreturn]] void handle_syscall(context_t* ctx, uint64 num);
void syscall_init(void);
#endif