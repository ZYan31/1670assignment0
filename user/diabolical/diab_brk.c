// Diabolical program: execute a software breakpoint instruction.
//
// `brk #0` is the AArch64 software breakpoint instruction.  It generates a
// synchronous debug exception with EC=0x3C ("BRK instruction execution in
// AArch64 state").  Under a debugger this would pause execution.
//
// A user process has no business executing BRK -- it's used by debuggers to
// set breakpoints in kernel-level code.

#include "diab_delay.h"
#include "u_common.h"
#include "u_syscalls.h"

int main() {
  diab_delay();  // let the rest of the system get going first

  // brk #0: synchronous debug exception, EC=0x3C.
  asm volatile("brk #0");

  // The kernel must kill this process before reaching here.
  printf("diab_brk: diabolical `brk` instruction ran!\r\n");
  return 0;
}
