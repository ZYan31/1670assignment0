#include "proc.h"
#include "elf.h"
#include "types.h"
#include "userprogs.h"   // find_program(), struct proginfo

// The programs init() starts at boot, in this order. idle MUST be first so it
// gets PID 0. Everything else is launched from userspace (term spawns them).
//
// Keep this array global and named exactly `boot_programs`: the build's
// user/gen_user_symbols.py reads it from the kernel ELF to tell GDB which
// program runs as which PID.
const char* boot_programs[] = {"idle", "vtty"};

void init() {
    for (uint64 i = 0; i < sizeof(boot_programs) / sizeof(boot_programs[0]); i++) {
        struct proginfo* prog = find_program(boot_programs[i]);
        if (prog == nullptr) {
            panic("init: there is no program called '%s'", boot_programs[i]);
        }
        struct proc* p = allocproc();
        // No strcpy in this freestanding kernel: bounded manual copy of the name.
        uint32 j = 0;
        while (prog->name[j] != '\0' && j < PROCNAME_MAXLEN - 1) {
            p->procName[j] = prog->name[j];
            j++;
        }
        p->procName[j] = '\0';
        load_elf_into_proc((struct elf_header*)prog->elf_start, p);
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
