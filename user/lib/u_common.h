#ifndef __LIB_U_COMMON_H
#define __LIB_U_COMMON_H

#include "u_klib.h"
#include "u_types.h"
#include "u_fb.h"
#include "u_syscalls.h"

void printf(const char* fmt, ...);
//void yield();
char getc();
int hasc();
// get_canvas/uptime_ms/get_proc_info trampolines removed -- use the syscalls
// init_canvas(), ticks(), process_info() (declared in u_syscalls.h) instead.
// wait_for_vsync() is a syscall now; its stub is declared in u_syscalls.h.

// Must match struct proc_info in kernel/proc.h (identical layout). name[] mirrors
// the kernel's PROCNAME_MAXLEN (32); the kernel fills these in via process_info().
struct proc_info {
    uint32 pid;
    char   name[32];
    char   state[16];
    uint32 ppid;
};

#endif  // __LIB_U_COMMON_H
