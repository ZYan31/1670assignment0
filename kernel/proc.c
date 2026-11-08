#include "proc.h"
#include "types.h"
#include "elf.h"

struct proc process_table[NPROC];
struct proc* current_process;
// static uint32 nwin = 0;   // moved to init(): windows are laid out there now

struct proc *allocproc(){
    //preempt_disable();
    struct proc *p = 0;
    for (uint32 i = 0; i < NPROC; i++){
        if (process_table[i].state == UNUSED){
            p = &process_table[i];
            p -> state = USED;
            p -> processID = i;
            // Each process owns a fixed 1MiB slice of RAM keyed by its PID.
            // sp stack pointer starts at the top of that slice.
            p -> stack = (void*)((uint64)PROC_START + (uint64)(i + 1) * PROC_SIZE);
            p -> kernel_stack = (void*)((uint64)KSTACKS_BASE+ (uint64)(i + 1) * KSTACK_SIZE);
            // Earlier-loaded procs rank above the later ones
            p -> priority = (i+1) * PRIORITY_SCALE;
            rb_init(&p->input_buf);
            p->usesBuf = false;
            //initializing the canvases for each process via framebuffer.
            struct fb_info* fb = fb_get();
            //address of base of proc mem + canvas offset.
            p->proc_canvas = (struct canvas*)((void*)((uint64)PROC_START + 
            (uint64)(i* PROC_SIZE)) + CANVAS_OFFSET);
            *p->proc_canvas = (struct canvas){
                .pixels = (void*)((uint64)PROC_START + (uint64)(i* PROC_SIZE)) 
                + CANVAS_OFFSET + sizeof(struct canvas),
                .height = fb->height/2 - 4,
                .width = fb->width/2 - 4,
                .stride = fb->width/2 - 4 
            };
            p->canvas_pixels = (void*)((uint64)PROC_START +
            (uint64)(i* PROC_SIZE)) + CANVAS_OFFSET + sizeof(struct canvas);
            p->usesScreen = false;
            p->uses_vsync = false;
            p->fd_table[0] = &console;
            console.ref_ct++;              // one more user of the console
            p->fd_table[1] = &console;
            console.ref_ct++;
            for(int i = 2; i < MAX_NUM_FDS; i++){
                p->fd_table[i] = nullptr;
            }
            break;
        }
    }
    if (!p){
        panic("allocproc: no free slot (NPROC=%d)", NPROC);
    }
    //preempt_enable();
    return p;
};

// Map a file-descriptor number to the file it refers to in the current
// process, or nullptr if fd is out of range or not open. Used by every
// file-related system call.
struct file* fd_lookup(int fd){
    if (fd < 0 || fd >= MAX_NUM_FDS) return nullptr;
    return current_process->fd_table[fd];
}

// Release every open file descriptor held by p: drop one reference on each
// open file and null the slot. Called when a process exits or is killed.
void proc_close_fds(struct proc* p){
    for (int i = 0; i < MAX_NUM_FDS; i++){
        if (p->fd_table[i]){
            p->fd_table[i]->ref_ct--;      // one fewer user of that file
            p->fd_table[i] = nullptr;
        }
    }
}

static const char* procstate_name(enum procstate s){
    switch (s){
        case UNUSED:   return "UNUSED";
        case USED:     return "USED";
        case RUNNABLE: return "RUNNABLE";
        default:   return "RUNNING";
    }
}

struct canvas* get_canvas(){
    struct proc* p = current_process;
    return p->usesScreen ? p->proc_canvas : nullptr;
}

// Copy one process-table slot into a caller-provided buffer. Returns 1 if the
// slot is in use (and *out is filled), 0 for an empty slot or a bad index.
// This is the whole user-visible view of the process table: pid, name, and a
// human-readable state string -- nothing that would let a user program reach
// into kernel structures.
int get_proc_info(uint32 index, struct proc_info* out){
    if (index >= NPROC) return 0;
    struct proc* p = &process_table[index];
    if (p->state == UNUSED) return 0;

    out->pid = p->processID;
    out->ppid = 0;              // no parent/child relationship in this kernel

    uint32 i = 0;
    for (; i < PROCNAME_MAXLEN - 1 && p->procName[i]; i++)
        out->name[i] = p->procName[i];
    out->name[i] = '\0';

    const char* s = procstate_name(p->state);
    uint32 j = 0;
    for (; j < sizeof(out->state) - 1 && s[j]; j++)
        out->state[j] = s[j];
    out->state[j] = '\0';

    return 1;
}

void print_process_table(){
    printf("Process table:\r\n");
    for (uint32 i = 0; i < NPROC; i++){
        struct proc* p = &process_table[i];
        if (p->state == UNUSED) continue;
        void* load = (void*)((uint64)PROC_START + (uint64)p->processID * PROC_SIZE);
        printf("pid %d  \"%s\"  load %p  entry %p  %s", p->processID,
             p->procName, load, p->entryPoint, procstate_name(p->state));
        if (p->usesScreen)
            printf("  win [%d,%d %dx%d]\r\n",
                   p->window.x, p->window.y, p->window.w, p->window.h);
        else
            printf("  (no window)\r\n");
    }
};
