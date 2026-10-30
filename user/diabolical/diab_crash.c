// Diabolical program: execute an undefined (unallocated) instruction.
//
// The bit pattern 0xFFFFFFFF does not correspond to any valid AArch64
// instruction.  Executing it causes the CPU to raise a synchronous exception
// with EC=0x00 ("Unknown reason / undefined instruction").
//
// This simulates a buggy or maliciously crafted executable that contains
// garbage instructions.

#include "diab_delay.h"
#include "u_common.h"
#include "u_syscalls.h"

int main() {
  diab_delay();  // let the rest of the system get going first

  // 0xFFFFFFFF is not a valid AArch64 encoding; the CPU raises an Undefined
  // Instruction exception (EC=0x00) when it tries to decode it.
  asm volatile(".word 0xFFFFFFFF");

  printf("diab_crash: diabolical bad instruction ran!\r\n");
  return 0;
}
