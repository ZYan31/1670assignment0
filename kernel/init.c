#include "proc.h"
#include "elf.h"
#include "types.h"

// Defined in kernel.c
extern char* executables[];
extern char* executable_names[];
extern bool  executable_draws[];   // index-aligned: does program i get a window?

// Added in Project 4. The diab_* programs (in user/diabolical/) misbehave on
// purpose (Quest 1); snake is a program we compiled for you (Quest 4, Part D),
// which the build embeds from user/prebuilt/snake.elf. basic_serial exists
// once you create user/basic_serial.c (Quest 3).
extern unsigned char _binary_user_diab_brk_elf_start[];
extern unsigned char _binary_user_diab_irqoff_elf_start[];
extern unsigned char _binary_user_diab_timer_elf_start[];
extern unsigned char _binary_user_diab_vectors_elf_start[];
extern unsigned char _binary_user_diab_crash_elf_start[];
extern unsigned char _binary_user_diab_stack_elf_start[];
extern unsigned char _binary_user_basic_serial_elf_start[];  // Quest 3
extern unsigned char _binary_user_snake_elf_start[];  // Quest 4 (provided)

// NOTE: this still loads from the executables[]/executable_names[] arrays in
// kernel.c. The Q5 quest rewrites it to use find_program() (userprogs.h) and
// drop those arrays -- done as the next step, after the merge.
void init() {
    // static uint32 nwin = 0;   // windows are now claimed on demand via init_canvas()
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
