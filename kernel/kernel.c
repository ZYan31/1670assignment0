// #include "gpio.h"
// #include "uart.h"
#include "drivers/uart.h"
#include "memlayout.h"
#include "printf.h"
#include "types.h"
#include "utils.h"
#include "init.h"
#include "proc.h"
#include "elf.h"
#include "exceptions.h"
#include "interrupts.h"
#include "scheduling.h"
#include "drivers/timer.h"
#include "drivers/timer.h"

typedef void (*vprintf_fn)(const char*, va_list);
//making a printf_fn that is the right space/size for the pointer.
extern char _binary_user_hello_elf_start[];    // Linker-generated, already exists in `init.c`
extern char _binary_user_counter_elf_start[];         // same
extern char _binary_user_primecheck_elf_start[]; // same
// extern char _binary_user_pi_elf_start[];
// extern char _binary_user_squares_elf_start[];

// All executables available
char* executables[] = {
    _binary_user_hello_elf_start,
    _binary_user_counter_elf_start,
    _binary_user_primecheck_elf_start,
    nullptr
};

// Names of the programs for debug output
char* executable_names[] = {"hello", "counter", "primecheck", nullptr};

void klib_init() {
    *(void(**) (const char*, va_list))F_VPRINTF = vprintf;
   //*(void(**) (void))F_YIELD = vyield;
   *(void (**)(void))F_YIELD = yield_entry;
}

void kernel_main(void) {
	uart_init();
    gpio_init();
    klib_init();
    printf("Hello world from Duckie!\r\n");
    init();
    exception_init(exception_vector_table);
    enable_interrupt_controller();
    enable_interrupts();
    timer_init(scheduler);
    print_process_table();
    printf("Starting pid 0 (hello)");
    //void (*primecheck)(void) = (void (*)(void))(process_table[2].entryPoint);
    //primecheck();
    current_process = &process_table[0];
    //asm volatile("svc #0");
    restore_context(process_table[0].context);
}

