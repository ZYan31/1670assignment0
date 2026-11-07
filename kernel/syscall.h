#ifndef SYSCALL_H
#define SYSCALL_H
#define SYS_yield 0
#define SYS_ticks 1
#define SYS_getpid 5
#include "types.h"
#include "proc.h"
#include "scheduling.h"
#include "drivers/timer.h"
#include "context.h"
#include "debug.h"
[[noreturn]] void handle_syscall(context_t* ctx, uint64 num);
void syscall_init(void);
#endif