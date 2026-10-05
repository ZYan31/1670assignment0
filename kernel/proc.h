#ifndef _PROC_H
#define _PROC_H

#include "context.h"
#include "types.h"
#include "printf.h"
#include "limits.h"
#include "memlayout.h"
#define AGING_RATE 1
#define PRIORITY_SCALE 10

enum procstate {UNUSED, USED, RUNNABLE, RUNNING };
struct proc {
    enum procstate state;
    uint32 processID;
    uint32 age;
    uint32 priority;
    uint64 cpu_ticks;   // timer ticks this process has owned the CPU (for measurement)
    char procName[PROCNAME_MAXLEN];
    void* entryPoint;
    void* stack;
    context_t* context;
};

extern struct proc  process_table[NPROC];
extern struct proc *current_process;

struct proc *allocproc();
void print_process_table();

#endif  // _PROC_H