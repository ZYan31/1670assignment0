#include "constant.h"

.section .text                      // 1. Put the table in the code segment
.globl exception_vector_table       // 2. Make the exception vector table visible to C code
.globl exception_init

exception_init:
    msr VBAR_EL1, x0
    ret

//can't just call yield_entry because 
irq_handler:
    sub sp, sp, #S_FRAME_SIZE
    stp x29, x30, [sp]
    bl save_context
    mov x0, sp
    bl handle_interrupt
    b .

sync_handler:
    sub sp, sp, #S_FRAME_SIZE
    stp x29, x30, [sp]
    bl save_context
    mov x0, sp
    bl handle_sync_exception
    b .

.balign 0x800                       // 3. Align the table to a 2 KiB boundary, as hardware requires
exception_vector_table:             // 4. Table that defines the table and its memory location
// Current EL, using SP_EL0 (unused in our OS)
    .balign 0x80
    b .                             // 0x000: Synchronous Exceptions
    .balign 0x80
    b .                             // 0x080: IRQ
    .balign 0x80
    b .                             // 0x100: FIQ
    .balign 0x80
    b .                             // 0x180: SError
// Current EL, using SP_ELx
    .balign 0x80
    b sync_handler                  // 0x200: Synchronous Exceptions
    .balign 0x80
    b irq_handler                   // 0x280: IRQ
    .balign 0x80
    b .                             // 0x300: FIQ
    .balign 0x80
    b .                             // 0x380: SError
// Lower EL, in AArch64
    .balign 0x80
    b .                             // 0x400: Synchronous Exceptions
    .balign 0x80
    b .                             // 0x480: IRQ
    .balign 0x80
    b .                             // 0x500: FIQ
    .balign 0x80
    b .                             // 0x580: SError
// Lower EL, in AArch32
    .balign 0x80
    b .                             // 0x600: Synchronous Exceptions
    .balign 0x80
    b .                             // 0x680: IRQ
    .balign 0x80
    b .                             // 0x700: FIQ
    .balign 0x80
    b .                             // 0x780: SError 

