
#include "constant.h"
#include "interrupts.h"
#include "context.h"
#include "printf.h"
#include "utils.h"
#include "drivers/timer.h"
#include "proc.h"

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

void handle_interrupt(context_t* ctx){
    //tracker
    timer +=1;
    //if(timer %100 ==0 ) printf("100 ticks have passed: %d", timer);
    if (timer == 1000) {   // ~10 s at a 10 ms quantum: dump the CPU split once
        printf("\r\n=== CPU over %d ticks ===\r\n", (int)timer);
        for (uint32 i = 0; i < NPROC; i++)
            if (process_table[i].state != UNUSED)
                printf("%s: %d ticks (%d%%)\r\n", process_table[i].procName,
                       (int)process_table[i].cpu_ticks,
                       (int)(process_table[i].cpu_ticks * 100 / timer));
    }

    //actual
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
    } else {
        panic("unexpected interrupt source: %p", (void*)(uint64)source);
    }
    restore_context(ctx);
};

void enable_interrupt_controller(void) {
    // Enable the ARM generic timer for core 0 by setting bit 1 of
    // the CORE0_TIMER_IRQ_CTRL register.
    mmio_write32(CORE0_TIMER_IRQ_CTRL, (0b1 << 1));
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
