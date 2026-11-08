#include "syscall.h"
#include "proc.h"
#include "scheduling.h"
#include "drivers/timer.h"
#include "context.h"
#include "debug.h"
#include "uaccess.h"
#include "drivers/uart.h"
#include "console.h"
#include "ringbuf.h"
#include "drivers/fb.h"

static int64 (*syscall_table[64])(context_t* ctx);

static int64 sys_yield(context_t* ctx){
    DEBUG(printf("yield: %s (pid %d)\r\n", current_process->procName,
  current_process->processID));
    handle_yield(ctx);
    return 0;
}

static int64 sys_ticks(context_t* ctx){
    (void)ctx;
    return (int64)uptime_ms();
}

static int64 sys_getpid(context_t* ctx){
    (void)ctx;
    return (int64)(current_process->processID);
}

static int64 sys_write(context_t* ctx){
    int fd = ctx->regs[0]; 
    const void* buf = (void*)ctx->regs[1]; 
    size_t n = ctx->regs[2];
    if(user_range_ok(buf, (uint64)n)){return -1;}
    struct file* f = fd_lookup(fd);
    char kbuf[128];
    if(!f){return -1;}
    size_t total = n;                     // save: the loop consumes n
    if(f->ft == FILE_CONSOLE){
        uint64 start = (uint64)buf;
        while(n > 0){
            uint64 len = (n < 128) ? n : 128;
            copy_from_user(kbuf, (void*)start, len);
            for (uint64 i = 0; i < len; i++){uart_send(kbuf[i]);}
            n -= len;
            start += len;
        }
    }
    return (int64)total;                  // number of bytes written
}

static int64 sys_read(context_t* ctx){
    int fd = ctx->regs[0];
    void* buf = (void*)ctx->regs[1];
    size_t n = ctx->regs[2];
    if(user_range_ok(buf, (uint64)n)){return -1;}   // reject bad range
    struct file* f = fd_lookup(fd);
    if(!f){return -1;}                               // reject bad fd
    if(n == 0){return 0;}                            // 0-byte read returns at once
    char kbuf[128];
    uint64 got = 0;
    if(f->ft == FILE_CONSOLE){
        current_process->usesBuf = true;
        uint64 start = (uint64)buf;
        // Block until at least one byte is available; console_getc() does the
        // waiting (and foreground/usesBuf bookkeeping) and returns that byte.
        kbuf[0] = console_getc();
        uint64 len = 1;
        for(;;){
            // Take only what is already buffered, up to n -- no more blocking.
            while(len < 128 && got + len < n &&
                  current_process->input_buf.count > 0){
                kbuf[len++] = rb_pop(&current_process->input_buf);
            }
            if(copy_to_user((void*)(start + got), kbuf, len)){return -1;}
            got += len;
            len = 0;
            if(got >= n || current_process->input_buf.count == 0){break;}
        }
    }
    return (int64)got;                               // how many bytes copied
}

static int64 peek(context_t* ctx){
    int fd = ctx->regs[0];
    struct file* f = fd_lookup(fd);
    if(!f){return -1;}                               // reject bad fd
    if(f->ft == FILE_CONSOLE){
        current_process->usesBuf = true;
        return (int64)current_process->input_buf.count;
    }
    return -1; 
}

static int64 process_info(context_t* ctx){
    struct proc_info* buf = (struct proc_info*)ctx->regs[0];
    int max = (int)ctx->regs[1];
    if (max < 0 ){return -1;}
    size_t n = max*sizeof(struct proc_info);
    if(user_range_ok(buf, (uint64)n)){return -1;}
    int filled = 0;
    for (int i = 0 ; i < NPROC && filled < max; i++){
        struct proc_info tmp;
        if (get_proc_info(i, &tmp)){                 // 1 = slot in use
            if (copy_to_user(&buf[filled], &tmp, sizeof(struct proc_info))){return -1;}
            filled ++;                               // copy succeeded (returned 0): count it
        }
    }
    return filled;
}

static int64 init_canvas(context_t* ctx){
    (void)ctx;                                           // init_canvas() takes no args
    struct proc* p = current_process;
    struct fb_info* fb = fb_get();
    if (!fb) return 0;                                   // no display -> NULL
    if (p->usesScreen) return (int64)p->proc_canvas;     // already has a window

    // Mark the grid cell each live window occupies (derived from its rect).
    uint32 cw = fb->width/2, ch = fb->height/2;
    bool used[4] = {0,0,0,0};
    for (uint32 i = 0; i < NPROC; i++){
        struct proc* q = &process_table[i];
        if (!q->usesScreen) continue;
        int c = (q->window.x >= cw ? 1 : 0) + (q->window.y >= ch ? 2 : 0);
        used[c] = true;
    }
    int cell = -1;
    for (int c = 0; c < 4; c++) if (!used[c]) { cell = c; break; }
    if (cell < 0) return 0;                              // all 4 taken -> NULL

    // Claim the cell: place the window and turn drawing on.
    p->window = (struct rect){ .x = cw*(cell%2)+2, .y = ch*(cell/2)+2,
                               .w = cw-4, .h = ch-4 };    // 2px border on every side
    p->usesScreen = true;
    return (int64)p->proc_canvas;                        // address in proc's own memory
}

static int64 sys_wait_for_vsync(context_t* ctx){
    (void)ctx;                      // wait_for_vsync() takes no args
    wait_for_vsync();
    return 0;
}

// static int64 spawn(context_t* ctx){
//     char* name = ctx->regs[0];
//     int stdin_fd = ctx->regs[1];
//     int stdout_fd = ctx->regs[2];

// }

// static int64 exit(context_t* ctx){
//     char* name = ctx->regs[0];
// }

// static int64 wait(context_t* ctx){
//     char* name = ctx->regs[0];
// }

// Placeholder stubs for later syscalls. Commented out until implemented:
// empty non-void bodies + unused ctx trip -Werror.
// static int64 pipe(context_t* ctx){
//
// }
//
// static int64 close(context_t* ctx){
//
// }
//
// static int64 open_console(context_t* ctx){
//
// }

[[noreturn]] void handle_syscall(context_t* ctx, uint64 exc){
    current_process->context = ctx;
    int64 ret;
    if (exc >= 64 || !syscall_table[exc])
        ret = -1;                      
    else
        ret = syscall_table[exc](ctx);  
    ctx->regs[0] = ret;                  
    restore_context(ctx);               
}

void syscall_init() {
    syscall_table[SYS_ticks] = sys_ticks;
    syscall_table[SYS_yield] = sys_yield;
    syscall_table[SYS_getpid] = sys_getpid;
    syscall_table[SYS_write] = sys_write;
    syscall_table[SYS_read] = sys_read;
    syscall_table[SYS_peek] = peek;
    syscall_table[SYS_process_info] = process_info;
    syscall_table[SYS_init_canvas] = init_canvas;
    syscall_table[SYS_wait_for_vsync] = sys_wait_for_vsync;
}