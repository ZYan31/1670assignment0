// Diabolical program: disable timer, so that timer interrupts stop

#include "diab_delay.h"
#include "u_common.h"
#include "u_syscalls.h"

int main() {
  diab_delay();  // let the rest of the system get going first

  // Turn off the ARM generic timer that drives preemption: writing 0 to its
  // control register clears its ENABLE bit (bit 0). This is one instruction
  // with the zero register as its source, so there is no scratch register
  // that the compiler could change between two asm statements.
  asm volatile("msr CNTP_CTL_EL0, xzr");

  printf("diab_timer: diabolical program disabled timer!\r\n");
  while (true) {
  }
  return 0;
}
