#include "memlayout.h"
#include "elf.h"
#include "printf.h"
// Program entry points (given by the linker)
extern unsigned char _binary_user_squares_elf_start[]; // the ELF for squares.c is there.
extern unsigned char _binary_user_pi_elf_start[];
extern unsigned char _binary_user_primecheck_elf_start[];

void run(void* executable, void* proc_address, char* name) {
    printf("Loading %s...\r\n", name);
    void* entry_point = load_elf(executable, proc_address);
    void (*f)(void) = (void (*)(void))entry_point;
    f();
}

void init() {
    run((void*)_binary_user_squares_elf_start, (void*)PROC_START, "SQUARES");
    run((void*)_binary_user_pi_elf_start, (void*)PROC_START, "PI");
    run((void*)_binary_user_primecheck_elf_start, (void*)PROC_START, "PRIMECHECK");
    //casting to void*
}
void exec(){
    // void (*vprintf_fn)(void) = (void (*)(void))F_VPRINTF;
    // vprintf();
}
