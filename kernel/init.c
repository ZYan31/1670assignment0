#include "proc.h"
#include "elf.h"
#include "types.h"

// Defined in kernel.c
extern char* executables[];
extern char* executable_names[];

void init() {
    // [...]
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
    }
    // [...]
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
