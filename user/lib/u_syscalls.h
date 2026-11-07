#ifndef U_SYSCALLS_H
#define U_SYSCALLS_H
#include "u_types.h"
int yield(void);
uint64 ticks(void);
void exit(int status);
int getpid(void);
#endif