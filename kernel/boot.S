#include "memlayout.h"

.section ".text.boot"

.globl _start
_start:
	mrs	x0, mpidr_el1		// Get current core ID into x0
	and	x0, x0, #0xFF		// Check processor id - //mask out the upper bits
	cbz	x0, processor0		// Hang for all non-primary CPU, jump if we are on core 0
	b	loop

processor0:
    adr x0, bss_begin       // Zero out the .bss segment of global variables
    adr x1, bss_end         // Save address of .bss end in x1
    sub x1, x1, x0          // Compute len of .bss segment by x1 - x0
    bl memzero              // Zero out the .bss segment of global variables
    mov sp, INITIAL_KERNEL_STACK
    bl kernel_main

loop: 
	b 	loop


