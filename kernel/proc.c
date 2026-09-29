#include "proc.h"
#include "types.h"
#include "elf.h"

struct proc process_table[NPROC];
struct proc* current_process;

struct proc *allocproc(){
    //preempt_disable();
    struct proc *p = 0;
    for (uint32 i = 0; i < NPROC; i++){
        if (process_table[i].state == UNUSED){
            p = &process_table[i];
            p -> state = USED;
            p -> processID = i;
            // Each process owns a fixed 64 KiB slice of RAM keyed by its PID.
            // The stack grows down, so start sp at the top of that slice.
            p -> stack = (void*)((uint64)PROC_START + (uint64)(i + 1) * PROC_SIZE);
            break;
        }
    }
    if (!p){
        panic("allocproc: no free slot (NPROC=%d)", NPROC);
    }
    //preempt_enable();
    return p;
};

static const char* procstate_name(enum procstate s){
    switch (s){
        case UNUSED:   return "UNUSED";
        case USED:     return "USED";
        case RUNNABLE: return "RUNNABLE";
        default:   return "RUNNING";
    }
}

void print_process_table(){
    printf("Process table:\r\n");
    for (uint32 i = 0; i < NPROC; i++){
        struct proc* p = &process_table[i];
        if (p->state == UNUSED) continue;
        void* load = (void*)((uint64)PROC_START + (uint64)p->processID * PROC_SIZE);
        printf("pid %d  \"%s\"  load %p  entry %p  %s\r\n",
               p->processID,
               p->procName,
               load,
               p->entryPoint,
               procstate_name(p->state));
    }
};
