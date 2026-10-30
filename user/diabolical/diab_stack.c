// Diabolical program: tricks the kernel into using a bad stack pointer.

#include "diab_delay.h"
#include "u_common.h"
#include "u_syscalls.h"

int main() {
  diab_delay();  // let the rest of the system get going first

  // Point sp at an address with no memory behind it (and not even 16-byte
  // aligned), then spin forever in a one-instruction loop that never touches
  // memory. The program do nothing harmful itself -- but the next timer
  // interrupt makes the kernel save its registers, and the question is: where?
  //
  // This has to be a single asm statement. The compiler does not know that
  // we changed sp, so it must not get to run any code of its own -- which
  // might use the stack -- between the switch and the loop.
  asm volatile(
      "ldr x0, =0xDEADBEEF\n"
      "mov sp, x0\n"
      "1: b 1b" ::
          : "x0", "memory");

  // we never get here
  return 0;
}
