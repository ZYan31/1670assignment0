#include "console.h"
#include "drivers/uart.h"

struct ringbuf input_buf;
void console_init(){
    rb_init(&input_buf);
}

void console_handle_input(char c){
    rb_push(&input_buf, c);
}