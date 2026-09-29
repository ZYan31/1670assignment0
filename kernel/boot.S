#include "memlayout.h"
#include "aarch64.h"

.section ".text.boot"

.globl _start
_start:
	mrs	x0, mpidr_el1		// Get current core ID into x0
	and	x0, x0, #0xFF		// Check processor id - //mask out the upper bits
	cbz	x0, processor0_el3		// Hang for all non-primary CPU, jump if we are on core 0
	b	loop
    
loop: 
	b 	loop

// Only processor 0 runs this code
processor0_el3:
    // Switch from EL3 to EL2
    ldr  x0, =SCR_VALUE_EL3  // Set values in System Control Register; see aarch64.h.
                             // NS=1 (non-secure), RW=1 (EL2 runs AArch64)
    msr  SCR_EL3, x0
    ldr  x0, =SPSR_VALUE_EL3 // Set values in the Saved Program Status Register (SPSR); see aarch64.h.
                             // return to EL2h, DAIF masked (no interrupts)
    msr  SPSR_EL3, x0
    adr  x0, processor0_el2
    msr  ELR_EL3, x0         // Store the address of processor0_el2 into the exception link register
                             // for EL3, meaning that `eret` will use it as the return address and
                             // jump to it.
    eret                     // Exception return; changes the exception level from EL3 to EL2 and
                             // jumps to the address in the ELR_EL3 register (which is processor0_el2)

processor0_el2:
    ldr   x0, =SCTLR_VALUE_MMU_DISABLED
    msr   SCTLR_EL1, x0  // Set values in System Control Register; see aarch64.h.

    ldr   x0, =HCR_VALUE_EL2
    msr   HCR_EL2, x0    // Set values in Hypervisor Configuration Register (HCR); see aarch64.h.

    ldr   x0, =SPSR_VALUE_EL2
    msr   SPSR_EL2, x0   // Set values in the Saved Program Status Register (SPSR); see aarch64.h.

    adr   x0, el1_entry
    msr   ELR_EL2, x0    // Store the address of el1_entry into the exception link register
                         // for EL2, meaning that `eret` will use it as the return address
                         // and jump to it.

    eret                 // Exception return; changes the exception level from EL2 to EL1 and jumps
                         // to address in the ELR_EL2 system register.

el1_entry:
    adr x0, bss_begin       // Zero out the .bss segment of global variables
    adr x1, bss_end         // Save address of .bss end in x1
    sub x1, x1, x0          // Compute len of .bss segment by x1 - x0
    bl memzero              // Zero out the .bss segment of global variables
    mov sp, INITIAL_KERNEL_STACK
    bl kernel_main

