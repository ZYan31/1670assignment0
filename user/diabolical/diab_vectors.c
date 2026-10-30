// Diabolical program: replace the kernel's exception vector table with our
// own.
//
// VBAR_EL1 (Vector Base Address Register, EL1) holds the base address of the
// exception vector table -- the dispatch table for ALL exceptions taken to
// EL1, including system calls and timer interrupts.
//
// The hardware dictates the table's shape: 16 entries of 0x80 bytes (32
// instructions) each, starting at a 2 KiB boundary (VBAR_EL1 ignores its low
// 11 bits). Which entry the CPU jumps to depends on the kind of exception
// (synchronous, IRQ, FIQ, SError, in that order within each group of four)
// and on where it was taken from:
//
//   +0x000  current EL, using SP_EL0
//   +0x200  current EL, using SP_ELx  (code running at EL1h)
//   +0x400  lower EL, AArch64         (code running at EL0)
//   +0x600  lower EL, AArch32
//
// If writing VBAR_EL1 succeeds, every exception and interrupt from then on
// runs *our* code, in the kernel at EL1.

#include "diab_delay.h"
#include "u_common.h"
#include "u_syscalls.h"

// Evil vector table. Each entry puts its own offset in x0 and jumps to
// evil_handler(), so the handler can tell which entry the CPU used. This has
// to be assembly: the C compiler would never lay out code like this.
asm(".pushsection .text\n"
    ".balign 0x800\n"  // the table must start at a 2 KiB boundary
    ".global evil_vectors\n"
    "evil_vectors:\n"
    ".set evil_offset, 0\n"
    ".rept 16\n"
    "  .balign 0x80\n"  // one entry every 0x80 bytes
    "  mov x0, #evil_offset\n"
    "  b evil_handler\n"
    "  .set evil_offset, evil_offset + 0x80\n"
    ".endr\n"
    ".popsection\n");

// Where every entry of our table lands: at EL1, with interrupts masked, on
// whatever stack was in use when the exception hit. There is nothing sane to
// return to, so we never return -- the machine is ours now.
//
// (`gnu::used` because only the asm above refers to this function, which
// the compiler cannot see.)
[[noreturn, gnu::used]] static void evil_handler(uint64 offset) {
  static const char* kinds[] = {"a synchronous exception", "an IRQ", "an FIQ",
                                "an SError"};
  uint64 current_el;
  asm volatile("mrs %0, CurrentEL" : "=r"(current_el));

  printf(
      "diab_vectors: caught %s (VBAR + 0x%x) in my own handler, at EL%d. "
      "I own this machine now!\r\n",
      kinds[(offset / 0x80) % 4], (int)offset, (int)((current_el >> 2) & 3));
  for (;;) {
  }
}

int main() {
  diab_delay();  // let the rest of the system get going first

  // Install evil table. `adr` computes its address relative to the PC, so
  // this works wherever the kernel loaded the program.
  asm volatile(
      "adr x0, evil_vectors\n"
      "msr VBAR_EL1, x0" ::
          : "x0");

  // The kernel must kill this process before reaching here.
  printf("diab_vectors: installed my own exception vector table!\r\n");

  // Evil table is in charge now: the next exception or interrupt -- this
  // `svc`, or a timer interrupt if one comes first -- runs the evil handler.
  asm volatile("svc #0");
  return 0;
}
