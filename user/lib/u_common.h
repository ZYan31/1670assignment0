#ifndef __LIB_U_COMMON_H
#define __LIB_U_COMMON_H

#include "u_klib.h"
#include "u_types.h"
#include "u_fb.h"

void printf(const char* fmt, ...);
void yield();
char getc();
int hasc();
void exit();
struct canvas* get_canvas();
void wait_for_vsync();   // block until the compositor puts the next frame up (F_VSYNC)

// Must match struct proc_info in kernel/proc.h (identical layout). name[] mirrors
// the kernel's PROCNAME_MAXLEN (32); the kernel fills these in across F_PS.
struct proc_info {
    uint32 pid;
    char   name[32];
    char   state[16];
};

uint64 uptime_ms(void);                                     // ms since boot (F_UPTIME)
int    get_proc_info(uint32 index, struct proc_info* out);  // one proc-table slot (F_PS)

#endif  // __LIB_U_COMMON_H
