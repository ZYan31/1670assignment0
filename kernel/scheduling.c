#include "context.h"
#include "proc.h"
#include "printf.h"
//void yield_entry(void) {}

#define SCHED_RR 0

struct proc* pick_next(){
#if SCHED_RR
    // Round-robin: next RUNNABLE after the current pid.
    for (uint32 i = 1; i <= NPROC; i++){
        uint32 pid = (current_process->processID + i) % NPROC;
        if (process_table[pid].state == RUNNABLE) return &process_table[pid];
    }
    panic("pick_next: no runnable process");
    return nullptr;   // unreachable; satisfies -Werror=return-type
#else
    int64 best = -1;          // signed so "nothing found" is detectable
    int   next = -1;
    for (uint32 i = 1; i < NPROC; i++){
        struct proc* p = &process_table[i];           // pointer, not a copy
        if (p->state == RUNNABLE && (int64)(p->priority + p->age) > best){
            best = (int64)(p->priority + p->age);
            next = i;
        }
    }
    if (next < 0) next = 0;
    return &process_table[next];
    //panic("pick_next: no runnable process"); //
    
#endif

    // for(int i = 1; i <= NPROC; i++){
    //     int pid = (current_process->processID + i)%NPROC;
    //     if(process_table[pid].state == RUNNABLE){
    //         return &process_table[pid];
    //     }
    // }
    // panic("No runnable processes!");
    // return nullptr;
    // for (int i = 0; i < NPROC; i++) {
    //     struct proc* p = &process_table[i];
    //     if (p) p->priority = (p->priority >> 1) + p->priority;
    // }
}

        // need to loop through processes to find max process
        // if max found, break. if no max, then trigger if condition to re-add priority

void resume(struct proc*p){
    current_process = p;
    current_process->state = RUNNING;
    current_process->age = 0;
    restore_context(current_process->context);
}

void scheduler(){
    //current_process->priority = 0;
    resume(pick_next());
}

void block(void* chan){
    current_process-> blocked_on = chan;
    current_process-> state = BLOCKED;
    yield_entry();
}

void wakeup(void* chan){
    for (int i = 0 ; i < NPROC; i++){
        if(process_table[i].state == BLOCKED && process_table[i].blocked_on == chan){
            process_table[i].state = RUNNABLE;
        }
    }
}

void handle_yield(context_t* ctx){
    current_process->context = ctx;
    if (current_process->state == RUNNING){
        current_process->state = RUNNABLE;
    }
    scheduler();
}


