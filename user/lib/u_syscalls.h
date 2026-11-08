#ifndef U_SYSCALLS_H
#define U_SYSCALLS_H
#include "u_types.h"
int yield(void);
uint64 ticks(void);
void exit(int status);
int getpid(void);
int64 write(int fd, const void* buf, size_t n);
int64 read(int fd, void* buf, size_t n);
int peek(int fd);
struct proc_info;   // defined in u_common.h; forward decl so the pointer type is known here
int process_info(struct proc_info* buf, int max);
struct canvas;      // defined in u_fb.h; forward decl so the return type is known here
struct canvas* init_canvas(void);
void wait_for_vsync(void);
#endif