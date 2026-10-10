#include "timer.h"
#include "types.h"
#include "uart.h"

// Desired scheduling quantum. CNTFRQ is in ticks/second, so ticks-per-quantum
// = freq / (1000 / QUANTUM_MS). We keep it as a divisor to stay in integers.
#define QUANTUM_MS 10                 // 50 ms (within the handout's 10-100 ms range)

static void (*timer_callback)(void); // what to run on each tick (the scheduler)
static uint64 ticks_per_quantum;     // CNTFRQ-derived count for one quantum

// --- raw system-register helpers ------------------------------------------

static inline uint64 read_cntfrq(void) {
    uint64 f;
    asm volatile ("mrs %0, cntfrq_el0" : "=r"(f));   // ticks per second (Hz)
    return f;
}

static inline void write_ctl(uint64 v) {
    asm volatile ("msr cntp_ctl_el0, %0" : : "r"(v));
    asm volatile ("isb");                            // let the write take effect
}

// --- public interface ------------------------------------------------------

void timer_set(uint64 interval_ticks) {
    // CNTP_TVAL_EL0 is a down-counter: the timer fires when it reaches 0, i.e.
    // `interval_ticks` ticks from now. This is how we turn a desired wall-clock
    // interval (ticks_per_quantum) into a hardware deadline without ever naming
    // an absolute time -- we just say "this many ticks from now".
    asm volatile ("msr cntp_tval_el0, %0" : : "r"(interval_ticks));
}

void timer_init(void (*callback)(void)) {
    timer_callback = callback;

    // How many ticks make up one quantum. CNTFRQ is fixed by the hardware
    // (e.g. ~19.2 MHz on the Pi 3), so ticks_per_quantum = freq * QUANTUM_MS/1000.
    uint64 freq = read_cntfrq();
    ticks_per_quantum = (freq * QUANTUM_MS) / 1000;

    timer_set(ticks_per_quantum);        // arm the first interval
    write_ctl(1);                        // ENABLE=1 (bit0), IMASK=0 (bit1) -> fires
}

void timer_interrupt(void) {
    // Re-arm FIRST so the next quantum starts counting immediately, then run the
    // callback (which context-switches away and never returns here).
    timer_set(ticks_per_quantum);
    if (timer_callback) {
        timer_callback();
    }
}
