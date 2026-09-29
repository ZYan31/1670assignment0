#include "context.h"
#include "proc.h"
#include "printf.h"
//void yield_entry(void) {}

struct proc* pick_next(){
    for(int i = 1; i <= NPROC; i++){
        int pid = (current_process->processID + i)%NPROC;
        if(process_table[pid].state == RUNNABLE){
            return &process_table[pid];
        }
    }
    panic("No runnable processes!");
    return nullptr;
}

void resume(struct proc*p){
    current_process = p;
    current_process->state = RUNNING;
    restore_context(current_process->context);
}

void scheduler(){
    resume(pick_next());
}

void handle_yield(context_t* ctx){
    current_process->context = ctx;
    current_process->state = RUNNABLE;
    scheduler();
}


