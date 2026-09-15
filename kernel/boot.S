#include "memlayout.h"

.section ".text.boot"

.globl _start
_start:
	mrs	x0, mpidr_el1		// get current core ID into x0
	and	x0, x0, #0xFF		// Check processor id - //mask out the upper bits
	cbz	x0, processor0		// Hang for all non-primary CPU
	b	loop

processor0:
    adr x0, bss_begin
    adr x1, bss_end
    sub x1, x1, x0
    bl memzero
    mov sp, INITIAL_KERNEL_STACK
    bl pi_main

loop: 
	b 	loop


