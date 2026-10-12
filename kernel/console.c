#include "console.h"
#include "drivers/uart.h"
#include "scheduling.h"
#include "printf.h"
int32 foreground_pid;

void console_init(){
    rb_init(&current_process->input_buf);
}

void deliver(char c){
    if (foreground_pid == 0) return;
    ringbuf_t* b = &process_table[foreground_pid].input_buf;
    rb_push(b, c);
    wakeup(b);
}

void console_handle_input(char c){
    static int s = 0;                                  // 0 normal, 1 saw ESC, 2 saw ESC '['
    if (c == 0x1B && s == 0) { s = 1; return; }        // hold ESC
    if (s == 1) {
        s = 0;
        if (c == '[') { s = 2; return; }               // hold ESC '['
        deliver(0x1B); console_handle_input(c); return;        // not it: release ESC, redo byte
    }
    if (s == 2) {
        s = 0;
        if (c == 'Z') {                                // Shift+Tab: rotate focus
            for (int i = 1; i <= NPROC; i++)
                if (process_table[(foreground_pid + i) % NPROC].usesBuf) { 
                foreground_pid =(foreground_pid + i) % NPROC; 
                printf("\nFocus PID: %d", foreground_pid); break; }
            return;}
        deliver(0x1B); deliver('['); console_handle_input(c); return;
    }
    deliver(c);
}

char console_getc(){
    while(current_process->input_buf.count == 0){
        if (foreground_pid == 0){
            foreground_pid = current_process->processID;
        }
        current_process->usesBuf = true;
        block(&current_process->input_buf);
    }
    return rb_pop(&current_process->input_buf);
}

int32 console_hasc(){
    if (foreground_pid == 0){
        foreground_pid = current_process->processID;
    }
    current_process->usesBuf = true;
    return current_process->input_buf.count;
}