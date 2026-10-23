#include "proc.h"
#include "elf.h"
#include "types.h"

// Defined in kernel.c
extern char* executables[];
extern char* executable_names[];
extern bool  executable_draws[];   // index-aligned: does program i get a window?

void init() {
    // [...]
    static uint32 nwin = 0;
    for (int i = 0; executables[i] != nullptr; i++) {
        struct proc* p = allocproc();
        // No strcpy in this freestanding kernel: bounded manual copy.
        uint32 j = 0;
        while (executable_names[i][j] != '\0' && j < PROCNAME_MAXLEN - 1) {
            p->procName[j] = executable_names[i][j];
            j++;
        }
        p->procName[j] = '\0';
        load_elf_into_proc((struct elf_header*)executables[i], p);
        p->state = RUNNABLE;
        p->usesScreen = executable_draws[i];
        if(p->usesScreen){
            struct fb_info* fb = fb_get();
            uint32 cw = fb->width/2, ch = fb->height/2;   // full cell: 640 x 360
            p->window = (struct rect){
                .x = cw*(nwin%2) + 2, .y = ch*(nwin/2) + 2,   // 2px border on every side
                .w = cw - 4, .h = ch - 4};                    // content 636 x 356
            nwin++;
        }
    }
}

// void run(void* executable, void* proc_address) {
//     void* entry_point = load_elf(executable, proc_address);
//     void (*f)(void) = (void (*)(void))entry_point;
//     f();
// }

// void init() {
//     run((void*)_binary_user_squares_elf_start, (void*)PROC_START);
//     run((void*)_binary_user_pi_elf_start, (void*)PROC_START);
//     run((void*)_binary_user_primecheck_elf_start, (void*)PROC_START);
//     //casting to void*
// }
