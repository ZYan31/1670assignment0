#include "timer.h"
#include "uart.h"
#include "../compositor.h"

static void (*timer_callback)(void); // what to run on a preemption (the scheduler)
static uint64 ticks_per_interrupt;   // CNTFRQ counts between interrupts (1/TIMER_HZ s)
static uint64 g_ticks;               // total timer interrupts since boot
static uint64 next_deadline;         // absolute CNTPCT of the next interrupt (drift-free)

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

// Current 64-bit counter value, and an absolute compare deadline. Unlike TVAL
// (a down-counter from "now"), CVAL fires when CNTPCT reaches an absolute value,
// so re-arming with deadline+period never loses the interrupt-entry latency.
static inline uint64 read_cntpct(void) {
    uint64 c;
    asm volatile ("mrs %0, cntpct_el0" : "=r"(c));
    return c;
}

static inline void write_cval(uint64 v) {
    asm volatile ("msr cntp_cval_el0, %0" : : "r"(v));
    asm volatile ("isb");
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

    // CNTFRQ is fixed by the hardware (e.g. ~19.2 MHz on the Pi 3). One timer
    // interrupt every 1/TIMER_HZ second is freq / TIMER_HZ counter ticks.
    uint64 freq = read_cntfrq();
    ticks_per_interrupt = freq / TIMER_HZ;

    // timer_set(ticks_per_interrupt);   // old: TVAL reload drifts (loses latency)
    next_deadline = read_cntpct() + ticks_per_interrupt;   // absolute first deadline
    write_cval(next_deadline);           // arm the first interval
    write_ctl(1);                        // ENABLE=1 (bit0), IMASK=0 (bit1) -> fires
}

void timer_interrupt(void) {
    // Re-arm FIRST so the next interval starts counting immediately. Advance the
    // absolute deadline by one period; if the handler ran long and we already
    // passed it, skip ahead to avoid a storm of back-to-back interrupts.
    // timer_set(ticks_per_interrupt);   // old: TVAL reload drifts (loses latency)
    next_deadline += ticks_per_interrupt;
    uint64 now = read_cntpct();
    if ((int64)(next_deadline - now) <= 0) next_deadline = now + ticks_per_interrupt;
    write_cval(next_deadline);
    g_ticks++;                       // one more timer tick (at TIMER_HZ) has elapsed

    // Job 2: preempt only on every QUANTUM_TICKS-th tick. On the other ticks we
    // fall through and the interrupt simply returns to the running process.
    compositor_tick();
    if (timer_callback && (g_ticks % QUANTUM_TICKS) == 0) {
        timer_callback();
    }
}

// Total timer ticks since boot (one tick per timer interrupt, at TIMER_HZ).
uint64 timer_ticks(void) {
    return g_ticks;
}

// Milliseconds since boot. g_ticks counts TIMER_HZ ticks, so ms = ticks*1000/HZ.
// We convert here, in the kernel, so user programs never see the tick rate.
uint64 uptime_ms(void) {
    return (g_ticks * 1000) / TIMER_HZ;
}
