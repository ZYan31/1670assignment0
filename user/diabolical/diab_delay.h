// Shared by the diabolical programs: wait a moment before misbehaving.
//
// Every other program yields or blocks almost as soon as it starts, so
// without this, a diabolical program would strike within milliseconds of
// boot -- before the rest of the system has drawn anything -- and there
// would be nothing to see break.
//
// How long we wait has to be measured in time, not in loop iterations: with
// its caches off, a real Raspberry Pi runs a loop many times slower than
// QEMU does. We read the time from the ARM generic timer's counter
// (CNTPCT_EL0), which ticks CNTFRQ_EL0 times a second. At EL1 (Project 3)
// that always works. At EL0 it works only because your kernel allows it,
// by setting EL0PCTEN in CNTKCTL_EL1 (see CNTKCTL_VALUE in
// kernel/aarch64.h); if it does not, every diabolical program dies with
// a trapped system register access here, rather than with its trick.

#ifndef _DIAB_DELAY_H
#define _DIAB_DELAY_H

#include "u_types.h"

#define DIAB_DELAY_MS 1000

static inline void diab_delay(void) {
  uint64 hz, start, now;
  asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(hz));
  hz &= 0xFFFFFFFF;  // the upper 32 bits are reserved
  asm volatile("isb; mrs %0, CNTPCT_EL0" : "=r"(start));
  do {
    asm volatile("isb; mrs %0, CNTPCT_EL0" : "=r"(now));
  } while (now - start < hz / 1000 * DIAB_DELAY_MS);
}

#endif  // _DIAB_DELAY_H
