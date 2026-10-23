#ifndef _PROC_H
#define _PROC_H

#include "context.h"
#include "types.h"
#include "printf.h"
#include "limits.h"
#include "memlayout.h"
#include "ringbuf.h"
#include "drivers/fb.h"
#define AGING_RATE 1
#define PRIORITY_SCALE 10

enum procstate {UNUSED, USED, RUNNABLE, RUNNING, BLOCKED};
struct rect {
    uint32 x;
    uint32 y;
    uint32 h;
    uint32 w;
} __attribute__((aligned(8)));

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
    void* blocked_on;
    ringbuf_t input_buf;
    bool usesBuf;
    bool usesScreen;
    bool uses_vsync;    // set the first time this proc calls wait_for_vsync()
    struct canvas* proc_canvas;
    struct rect window;
    void* canvas_pixels;
};

extern struct proc  process_table[NPROC];
extern struct proc *current_process;

// A read-only snapshot of one process-table slot, handed to user programs via
// the F_PS trampoline (for `ps`). Its layout MUST match struct proc_info in
// user/lib/u_common.h. We copy the state as a string so user code needs neither
// the procstate enum nor its name table.
struct proc_info {
    uint32 pid;
    char   name[PROCNAME_MAXLEN];
    char   state[16];
};

struct proc *allocproc();
void print_process_table();
struct canvas* get_canvas();

// Fill *out with slot `index`; return 1 if that slot is in use, else 0.
int get_proc_info(uint32 index, struct proc_info* out);

#endif  // _PROC_H