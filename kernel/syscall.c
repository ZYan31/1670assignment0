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
#include "pipe.h"        // struct pipe, pipe_alloc(), NPIPE
#include "drivers/fb.h"
#include "userprogs.h"   // find_program(), struct proginfo, PROG_MAXNAME
#include "elf.h"         // load_elf_into_proc()
#include "userprogs.h"

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
    if(f->ft == FILE_PIPE_READ){return -1;}   // can't write a pipe's read end
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
    } else if(f->ft == FILE_PIPE_WRITE){
        struct pipe* pp = f->pipe;
        uint64 start = (uint64)buf;
        uint64 done = 0;
        while(done < total){
            // Wait for room; if every reader is gone the pipe is broken.
            while(pp->buf.count == R_CAP && pp->rd.ref_ct > 0){ block(pp); }
            if(pp->rd.ref_ct == 0){return -1;}        // broken pipe: no readers
            uint64 space = R_CAP - pp->buf.count;
            uint64 len = total - done;
            if(len > space){len = space;}
            if(len > 128){len = 128;}
            if(copy_from_user(kbuf, (void*)(start + done), len)){return -1;}
            for(uint64 i = 0; i < len; i++){ rb_push(&pp->buf, kbuf[i]); }
            done += len;
            wakeup(pp);                               // a reader may be waiting
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
    if(f->ft == FILE_PIPE_WRITE){return -1;}         // can't read a pipe's write end
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
    } else if(f->ft == FILE_PIPE_READ){
        struct pipe* pp = f->pipe;
        // Block until data arrives, unless every writer is gone (then EOF: 0).
        while(pp->buf.count == 0 && pp->wr.ref_ct > 0){ block(pp); }
        uint64 start = (uint64)buf;
        while(got < n && pp->buf.count > 0){
            uint64 len = 0;
            while(len < 128 && got + len < n && pp->buf.count > 0){
                kbuf[len++] = rb_pop(&pp->buf);
            }
            if(copy_to_user((void*)(start + got), kbuf, len)){return -1;}
            got += len;
        }
        wakeup(pp);                                  // a writer may be waiting for space
    }
    return (int64)got;                               // how many bytes copied
}

static int64 sys_peek(context_t* ctx){
    int fd = ctx->regs[0];
    struct file* f = fd_lookup(fd);
    if(!f){return -1;}                               // reject bad fd
    if(f->ft == FILE_CONSOLE){
        current_process->usesBuf = true;
        if (foreground_pid == 0 || is_ancestor((uint32)foreground_pid, current_process)){
            foreground_pid = current_process->processID;
        }
        return (int64)current_process->input_buf.count;
    } else if (f->ft == FILE_PIPE_READ){
        struct pipe* pp = f->pipe;
        return (int64)pp->buf.count;
    }
    return -1;
}

static int64 sys_process_info(context_t* ctx){
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

static int64 sys_init_canvas(context_t* ctx){
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

static int64 sys_spawn(context_t* ctx){
    // Copy the name into kernel memory first -- never walk a user pointer in
    // the kernel. Check the copy, terminate, THEN look the program up.
    char name[PROG_MAXNAME];
    if (copy_from_user(name, (void*)ctx->regs[0], PROG_MAXNAME)) return -1;
    name[PROG_MAXNAME - 1] = '\0';

    struct file* in  = fd_lookup((int)ctx->regs[1]);
    struct file* out = fd_lookup((int)ctx->regs[2]);
    struct proginfo* program = find_program(name);
    if (!program || !in || !out) return -1;      // validate BEFORE allocating

    struct proc* process = allocproc();
    if (!process) return -1;                      // table full: no panic, just fail

    process->fd_table[0] = in;  in->ref_ct++;     // child inherits parent's chosen fds
    process->fd_table[1] = out; out->ref_ct++;
    process->ppid = current_process->processID;
    load_elf_into_proc((struct elf_header*)program->elf_start, process);
    process->state = RUNNABLE;                     // last: child is fully built
    return (int64)process->processID;
}

// Non-static: the fault handler (interrupts.c) calls this directly to kill a
// faulting process. Declared in syscall.h. Every other handler stays static.
int64 sys_exit(context_t* ctx){
    int status = ctx->regs[0];
    current_process->usesBuf = false;
    current_process->usesScreen = false;
    current_process->state = ZOMBIE;
    current_process->ex_stat = status;
    proc_close_fds(current_process);
        if(foreground_pid == (int32)current_process->processID){
            int anc = closest_reader_ancestor(current_process);   // parent-chain reader
            if (anc >= 0){
                foreground_pid = anc;
            } else {
                foreground_pid = 0;                               // else next Shift+Tab reader
                for (int i = 1; i <= NPROC+1; i++){
                    if (process_table[(foreground_pid + i) % NPROC].usesBuf) {
                        foreground_pid =(foreground_pid + i) % NPROC; break;}
                }
            }
        }
    wakeup(current_process);
    scheduler();
    return 0;
}

static int64 sys_wait(context_t* ctx){
    int pid = ctx->regs[0];
    if (pid < 0 || pid >= NPROC){return -1;}
    struct proc* child = &process_table[pid];
    if(child->state == UNUSED|| child->ppid != current_process->processID){return -1;}
    while (child->state != ZOMBIE){block(child);}
    int64 ex_stat = child->ex_stat;
    child->state = UNUSED;
    return ex_stat;
}

// pipe(int* fds): make a pipe and hand back its two ends in the caller's two
// lowest free fd slots (read end low, write end high). Validate everything
// before touching the fd table so a failure leaves the process untouched.
static int64 sys_pipe(context_t* ctx){
    // int fds[2] = ctx->regs[0];                 // original draft
    int* ufds = (int*)ctx->regs[0];
    if(user_range_ok(ufds, 2 * sizeof(int))){return -1;}   // fds not in caller memory

    int lo = -1, hi = -1;                          // two lowest free descriptors
    for(int i = 0; i < MAX_NUM_FDS; i++){
        if(!current_process->fd_table[i]){
            if(lo < 0){ lo = i; }
            else { hi = i; break; }
        }
    }
    if(hi < 0){return -1;}                          // fewer than two free fds

    struct pipe* p = pipe_alloc();
    if(!p){return -1;}                              // no free pipe

    current_process->fd_table[lo] = &p->rd; p->rd.ref_ct++;
    current_process->fd_table[hi] = &p->wr; p->wr.ref_ct++;

    int fds[2] = { lo, hi };
    copy_to_user(ufds, fds, sizeof(fds));           // range already validated above
    return 0;
}

// close(fd): drop this descriptor. For a pipe end, waking the pipe lets a
// blocked peer notice the end went away (EOF for a reader, broken pipe for a
// writer); when both ends are gone the pipe returns to the pool.
static int64 sys_close(context_t* ctx){
    int fd = (int)ctx->regs[0];
    struct file* f = fd_lookup(fd);
    if(!f){return -1;}
    current_process->fd_table[fd] = nullptr;
    if(f->ref_ct > 0){ f->ref_ct--; }
    if(f->ft == FILE_PIPE_READ || f->ft == FILE_PIPE_WRITE){
        wakeup(f->pipe);
        if(f->pipe->rd.ref_ct == 0 && f->pipe->wr.ref_ct == 0){
            f->pipe->used = false;                  // both ends closed: reclaim
        }
    }
    return 0;
}

// Placeholder stubs for later syscalls. Commented out until implemented:
// empty non-void bodies + unused ctx trip -Werror.
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
    syscall_table[SYS_peek] = sys_peek;
    syscall_table[SYS_process_info] = sys_process_info;
    syscall_table[SYS_init_canvas] = sys_init_canvas;
    syscall_table[SYS_wait_for_vsync] = sys_wait_for_vsync;
    syscall_table[SYS_spawn] = sys_spawn;
    syscall_table[SYS_exit] = sys_exit;
    syscall_table[SYS_wait] = sys_wait;
    syscall_table[SYS_pipe] = sys_pipe;
    syscall_table[SYS_close] = sys_close;
}