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
extern char _binary_user_idle_elf_start[];
extern char _binary_user_logo_elf_start[];
extern char _binary_user_basic_elf_start[];
extern char _binary_user_hello_elf_start[];    // Linker-generated, already exists in `init.c`
extern char _binary_user_counter_elf_start[];         // same
extern char _binary_user_primecheck_elf_start[]; // same
extern char _binary_user_keytest_elf_start[];    // Quest 2: echoes keystrokes via getc()
extern char _binary_user_term_elf_start[];       // Quest 3 Part D: framebuffer terminal
extern char _binary_user_pong_elf_start[];       // Quest 3: bouncing box (tearing demo)
extern char _binary_user_clock_elf_start[];      // Quest 4: clock in its own window
extern char _binary_user_diab_brk_elf_start[];       // Project 4 Q1.
extern char _binary_user_diab_crash_elf_start[];
extern char _binary_user_diab_irqoff_elf_start[];
extern char _binary_user_diab_stack_elf_start[];
extern char _binary_user_diab_timer_elf_start[];
extern char _binary_user_diab_vectors_elf_start[];
int64 (*syscall_table[64])(context_t* ctx);
// extern char _binary_user_pi_elf_start[];
// extern char _binary_user_squares_elf_start[];

// All executables available
char* executables[] = {
    _binary_user_idle_elf_start,
    _binary_user_hello_elf_start,
    _binary_user_counter_elf_start,
    _binary_user_primecheck_elf_start,
    nullptr
};

// Names of the programs for debug output
char* executable_names[] = {"idle", "hello", "counter", "primecheck", nullptr};

// Which programs get a window. Must stay index-aligned with executables[].
bool executable_draws[] = {false, false, false, false};

void klib_init() {
    *(void(**) (const char*, va_list))F_VPRINTF = vprintf;
    //*(void(**) (void))F_YIELD = vyield;
    *(void (**)(void))F_YIELD = yield_entry;
    *(char (**)(void))F_GETC = console_getc;
    *(int32 (**)(void))F_HASC = console_hasc;
    *(uint64 (**)(void))F_UPTIME = uptime_ms;                       // Quest 3D: `uptime`
    *(int (**)(uint32, struct proc_info*))F_PS = get_proc_info;     // Quest 3D: `ps`
    *(struct canvas* (**)(void))F_GET_CANVAS = get_canvas;         // Quest 4A: `get_canvas`
    *(void (**)(void))F_VSYNC = wait_for_vsync;
}

void kernel_main(void) {
	uart_init();
    gpio_init();
    klib_init();
    printf("Hello world from Duckie!\r\n");
    exception_init(exception_vector_table);
    enable_interrupt_controller();
    enable_interrupts();
    printf("Starting pid 0 (hello)");
    console_init();
    fb_init();
    struct fb_info* fb = fb_get();
    for(uint32 y = 0 ; y < fb->height; y++){
        for(uint32 x = 0 ; x < fb->width; x++){
            *(fb->pixels + (y*fb->pitch_w + x)) = 0xFF0000FF;
        }
    }
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

