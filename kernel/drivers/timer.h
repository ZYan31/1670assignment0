#ifndef _DRIVERS_TIMER_H
#define _DRIVERS_TIMER_H

#include "types.h"

// ARM generic timer (EL1 physical timer, accessed via system registers).
// Used exclusively to preempt processes: when it fires, the registered
// callback (the scheduler) runs.

// Read the hardware tick rate, compute the per-quantum tick count, register
// the callback to invoke on each tick, enable the timer, and arm it.
void timer_init(void (*callback)(void));

// Arm the timer to fire `interval_ticks` ticks from now.
void timer_set(uint64 interval_ticks);

// Called when the timer fires: re-arm, then invoke the callback.
void timer_interrupt(void);

#endif  // _DRIVERS_TIMER_H
