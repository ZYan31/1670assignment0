#include "syscall.h"
#include "proc.h"

static int64 (*syscall_table[64])(context_t* ctx);

static int64 sys_yield(context_t* ctx){
    DEBUG(printf("yield: %s (pid %d)\r\n", current_process->procName,
  current_process->processID));
    handle_yield(ctx);
    return 0;
}

static int64 sys_ticks(context_t* ctx){
    (void)ctx;
    return (int64)uptime_ms();
}

static int64 sys_getpid(context_t* ctx){
    (void)ctx;
    return (int64)(current_process->processID);
}

[[noreturn]] void handle_syscall(context_t* ctx, uint64 exc){
    current_process->context = ctx;
    int64 ret;
    if (exc >= 64 || !syscall_table[exc])
        ret = -1;                      
    else
        ret = syscall_table[exc](ctx);  
    ctx->regs[0] = ret;                  
    restore_context(ctx);               
}

void syscall_init() {
    syscall_table[SYS_ticks] = sys_ticks;
    syscall_table[SYS_yield] = sys_yield;
    syscall_table[SYS_getpid] = sys_getpid;
}