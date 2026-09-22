// #include "gpio.h"
// #include "uart.h"
#include "drivers/uart.h"
#include "memlayout.h"
#include "printf.h"
#include "types.h"
#include "utils.h"
#include "init.h"

typedef void (*vprintf_fn)(const char*, va_list);
//making a printf_fn that is the right space/size for the pointer. 
void klib_init() {
    *(void(**) (const char*, va_list))F_VPRINTF = vprintf;
}
void kernel_main(void) {
	uart_init();
    gpio_init();
    klib_init();
    printf("Hello world from DemoOS!\r\n");
    init();
}

