#include "memlayout.h"
// Program entry points (given by the linker)
extern unsigned char _binary_user_squares_elf_start[];
extern unsigned char _binary_user_pi_elf_start[];
extern unsigned char _binary_user_primecheck_elf_start[];

void init() {
    
}
void exec(){
    // void (*vprintf_fn)(void) = (void (*)(void))F_VPRINTF;
    // vprintf();
}
