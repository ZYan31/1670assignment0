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
void kernel_main(void) {
	uart_init();
    gpio_init();
    *(((volatile vprintf_fn*)((F_VPRINTF)))) = vprintf;
    init();
}

