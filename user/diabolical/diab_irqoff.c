// Diabolical program: attempt to disable hardware interrupts.
#include "diab_delay.h"
#include "u_common.h"
#include "u_syscalls.h"

int main() {
  diab_delay();  // let the rest of the system get going first

  // Attempt to set all four DAIF mask bits, which would mask (disable) debug
  // exceptions, SError, IRQs and FIQs. With IRQs masked, no timer interrupt
  // can preempt us, and we never yield.
  asm volatile("msr daifset, #0xf" ::: "memory");

  // The kernel must kill this process before reaching here.
  printf("diab_irqoff: diabolical IRQ disabling succeeded!\r\n");
  while (true) {
  }
  return 0;
}
