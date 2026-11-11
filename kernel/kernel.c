// #include "gpio.h"
// #include "uart.h"
#include "drivers/uart.h"
#include "memlayout.h"
#include "printf.h"
#include "types.h"
#include "utils.h"
#include "init.h"
#include "types.h"
#include "proc.h"
#include "elf.h"
#include "exceptions.h"
#include "interrupts.h"
#include "scheduling.h"
#include "drivers/timer.h"
#include "drivers/timer.h"
#include "console.h"  
#include "aarch64.h"
#include "drivers/fb.h"
#include "syscall.h"

typedef void (*vprintf_fn)(const char*, va_list);
//making a printf_fn that is the right space/size for the pointer.
int64 (*syscall_table[64])(context_t* ctx);

// The boot program list and the per-program symbols now live in init.c, which
// looks programs up by name with find_program() (kernel/userprogs.h). The
// kernel no longer keeps an executables[] array or _binary_* externs here.

// Trampolines removed: every kernel service user programs once reached through
// a fixed F_* address is now a real system call. The kernel no longer publishes
// anything at a fixed address for user programs to find.
// void klib_init() { ... F_VPRINTF / F_YIELD / F_GETC / F_HASC / F_UPTIME /
//                        F_PS / F_GET_CANVAS / F_VSYNC installs ... }

void kernel_main(void) {
	uart_init();
    gpio_init();
    // klib_init();   // removed: no more fixed-address trampolines
    printf("Hello world from Duckie!\r\n");
    exception_init(exception_vector_table);
    enable_interrupt_controller();
    enable_interrupts();
    printf("Starting pid 0 (hello)");
    console_init();
    fb_init();
    struct fb_info* fb = fb_get();
    (void)fb;
    // Leftover debug fill: paints only one framebuffer half red, so the vsync
    // flip alternated red/white. Disabled; fb_init already clears both halves.
    // for(uint32 y = 0 ; y < fb->height; y++){
    //     for(uint32 x = 0 ; x < fb->width; x++){
    //         *(fb->pixels + (y*fb->pitch_w + x)) = 0xFF0000FF;
    //     }
    // }
    asm volatile("msr CNTKCTL_EL1, %0" :: "r"((uint64)CNTKCTL_VALUE));
    //NORMAL kernel
    init(); 
    timer_init(scheduler);
    print_process_table();
    syscall_init();
    current_process = &process_table[0];
    restore_context(process_table[0].context);
    
    // //TEST RINGBUFFER
    // printf("ringbuf test - type something\r\n");
    // while (1) {
    //     while (!rb_isEmpty(&input_buf)) {
    //         printf("%c", rb_pop(&input_buf));
    //     }
    //     asm volatile("wfi");
    // }

    // Previously written, discard.
    //void (*primecheck)(void) = (void (*)(void))(process_table[2].entryPoint);
    //primecheck();
    //asm volatile("svc #0");
}

