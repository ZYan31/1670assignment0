#ifndef _INTERRUPTS_H
#define _INTERRUPTS_H
#define CORE0_TIMER_IRQ_CTRL (ARM_LOCAL_PERIPHERALS_BASE + 0x40)
#define CORE0_INTERRUPT_SOURCE (ARM_LOCAL_PERIPHERALS_BASE + 0x60)

#include "memlayout.h"
#include "context.h"
// Add any necessary definitions here.

// Declare public functions like `handle_interrupt` and `enable_interrupts`
// here!

void handle_sync_exception(context_t* ctx);
void handle_interrupt(context_t* ctx);
void enable_interrupts(void);
void enable_interrupt_controller(void);
unsigned long disable_interrupts(void);
void restore_interrupts(unsigned long flags);
#endif  // _INTERRUPTS_H