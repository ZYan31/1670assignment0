#include "interrupts.h"

int64 timer;

void handle_sync_exception(context_t* ctx){
    uint64 esr;
    uint64 elr;
    uint64 far;
    asm volatile ("mrs %0, esr_el1" : "=r"(esr));   // read sysreg -> C var
    asm volatile("mrs %0, elr_el1" : "=r" (elr));
    asm volatile("mrs %0, far_el1" : "=r" (far));
    printf("ESR_EL1 = %p\r\n", (void*)esr);
    uint64 ec = (esr >> 26) & 0b111111;
    printf("ESR_EL1 31:26 = %p\r\n", (void*)ec);
    printf("ELR_EL1 = %p\r\n", (void*)elr);
    printf("FAR_EL1 = %p\r\n", (void*)far);
    printf("saved elr = %p   spsr = %p\r\n", (void*)ctx->elr_el1, (void*)ctx->spsr_el1);
    printf("x30 (lr) = %p   x29 (fp) = %p\r\n", (void*)ctx->lr, (void*)ctx->fp);
    printf("x0 = %p   x1 = %p\r\n", (void*)ctx->regs[0], (void*)ctx->regs[1]);
};

// Synchronous exception taken from EL0 (a user process did something that
// trapped). For now: dump the fault registers and halt.
void handle_el0_sync(context_t* ctx){
    (void)ctx;
    uint64 esr, elr, far, ex_class, syscall_num;
    struct proc* p = current_process;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    asm volatile("mrs %0, elr_el1" : "=r"(elr));
    asm volatile("mrs %0, far_el1" : "=r"(far));
    ex_class = ((esr>>26) & 0b111111);
    if(ex_class == ESR_EL1_EC_SVC_CAUSE){
        syscall_num = (esr & ESR_EL1_IMM_MASK);
        handle_syscall(ctx, syscall_num);
    } else {
        //print process which caused exception
        // how to get the process?
        printf("  Exception caused by: %s, PID: %d\r\n", p->procName, p->processID);
        printf("  Exception Class = %p\r\n", (void*)ex_class);
        //printf("  ESR_EL1 = %p\r\n", (void*)esr);
        printf("  ELR_EL1 = %p\r\n", (void*)elr);
        if(0x20 <= ex_class && ex_class <= 0x25){
            printf("  FAR_EL1 = %p\r\n", (void*)far);
        }
        proc_close_fds(p);   // release the dying process's file descriptors
        p->state = UNUSED;
        p->usesBuf = false;
        if(foreground_pid == (int32)p->processID){
            foreground_pid = 0;
            for (int i = 1; i <= NPROC+1; i++){
                if (process_table[(foreground_pid + i) % NPROC].usesBuf) { 
                    foreground_pid =(foreground_pid + i) % NPROC; break;}
            }
        }
        p->usesScreen = false;
        scheduler();
    }
    //panic("handle_el0_sync: unhandled EL0 synchronous exception");
}

void handle_interrupt(context_t* ctx){
    //tracker
    timer +=1;
    //if(timer %100 ==0 ) printf("100 ticks have passed: %d", timer);
    // if (timer == 1000) {   // ~10 s at a 10 ms quantum: dump the CPU split once
    //     printf("\r\n=== CPU over %d ticks ===\r\n", (int)timer);
    //     for (uint32 i = 0; i < NPROC; i++)
    //         if (process_table[i].state != UNUSED)
    //             printf("%s: %d ticks (%d%%)\r\n", process_table[i].procName,
    //                    (int)process_table[i].cpu_ticks,
    //                    (int)(process_table[i].cpu_ticks * 100 / timer));
    // }
    uint64 source = mmio_read32(CORE0_INTERRUPT_SOURCE);
    if (source & (0b1 << 1)){
        // This tick belonged to the process that was running.
        current_process->cpu_ticks++;
        // Age every waiting process (not the one running, still RUNNING here).
        for (uint32 i = 0; i < NPROC; i++)
            if (process_table[i].state == RUNNABLE)
                process_table[i].age += AGING_RATE;
        current_process->context = ctx;
        current_process->state = RUNNABLE;
        timer_interrupt();            // re-arm -> scheduler -> pick_next -> resume (no return)
    } else if (source & (0b1 << 8)) { // Gets from the GPU that peripheral IRQ is pending. 
        if (mmio_read32(IRQ_PENDING_2) & (0b1 << 25)) { // uart specifically. shift 25.
            uart_interrupt();
        }
    } else {
        panic("unexpected interrupt source: %p", (void*)(uint64)source);
    }
    restore_context(ctx);
};

void enable_interrupt_controller(void) {
    // Enable the ARM generic timer for core 0 by setting bit 1 of
    // the CORE0_TIMER_IRQ_CTRL register.
    mmio_write32(CORE0_TIMER_IRQ_CTRL, (0b1 << 1));
    mmio_write32(ENABLE_IRQS_2, (0b1 << 25));
};

void enable_interrupts(void){
    asm volatile ("msr daifclr, #2" ::: "memory");   // unmask IRQs
};

unsigned long disable_interrupts(void){
    unsigned long flags;
    asm volatile ("mrs %0, daif" : "=r"(flags));      // save current state
    asm volatile ("msr daifset, #2" ::: "memory");    // mask IRQs
    return flags;                                      // caller restores with this
};

void restore_interrupts(unsigned long flags){
    asm volatile ("msr daif, %0" :: "r"(flags) : "memory");  // put DAIF back
};
