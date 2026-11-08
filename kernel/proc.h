#ifndef _PROC_H
#define _PROC_H

#include "context.h"
#include "types.h"
#include "printf.h"
#include "limits.h"
#include "memlayout.h"
#include "ringbuf.h"
#include "drivers/fb.h"
#include "file.h"
#define AGING_RATE 1
#define PRIORITY_SCALE 10
#define MAX_NUM_FDS 16

enum procstate {UNUSED, USED, RUNNABLE, RUNNING, BLOCKED, ZOMBIE};
struct rect {
    uint32 x;
    uint32 y;
    uint32 h;
    uint32 w;
} __attribute__((aligned(8)));

struct proc {
    enum procstate state;       // scheduling state: UNUSED/USED/RUNNABLE/RUNNING/BLOCKED/ZOMBIE
    uint32 processID;           
    uint32 age;                 // ticks spent RUNNABLE but not run (drives aging/priority boost)
    uint32 priority;            // scheduling priority
    uint64 cpu_ticks;   // timer ticks this process has owned the CPU (for measurement)
    char procName[PROCNAME_MAXLEN];  // human-readable name (for ps / debug prints)
    void* entryPoint;           // initial PC: the ELF entry point
    void* stack;                // top of this proc's user stack (sp_el0 start) == top of its slot
    context_t* context;         // saved register state; set on every trap into the kernel
    void* blocked_on;           // when BLOCKED, the resource this proc is waiting on
    ringbuf_t input_buf;        // per-proc keyboard input ring buffer
    bool usesBuf;               // true if this proc consumes input_buf
    bool usesScreen;            // true if this proc draws to the framebuffer
    bool uses_vsync;    // set the first time this proc calls wait_for_vsync()
    struct canvas* proc_canvas; // this proc's drawing canvas (header + pixels)
    struct rect window;         // on-screen rectangle for its canvas
    void* canvas_pixels;        // backing pixel buffer for proc_canvas
    void* kernel_stack;         // top of this proc's kernel stack (used on trap into EL1)
    struct file* fd_table[MAX_NUM_FDS];
    uint32 ppid;
    uint32 ex_stat;
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
    uint32 ppid;
};

struct proc *allocproc();
void proc_close_fds(struct proc* p);  // release p's open fds (on exit/kill)
struct file* fd_lookup(int fd);       // current proc's fd -> file*, or null
void print_process_table();
struct canvas* get_canvas();

// Fill *out with slot `index`; return 1 if that slot is in use, else 0.
int get_proc_info(uint32 index, struct proc_info* out);

#endif  // _PROC_H