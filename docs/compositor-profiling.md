# Compositor profiling (Quest 5 Part A)

Instrumentation that was removed from `kernel/compositor.c` after it had served
its purpose. It counts, per real-time second: how many refreshes the compositor
did, and per window how many fresh canvas copies (`copies`) and second-half
"hold-back" copies (`held`) it performed. It compiles to nothing at
`DEBUG_LVL=0`, so it is only active in a `make DEBUG_LVL=1` build.

It uses the hardware counter (`CNTPCT_EL0`) rather than `timer_ticks()` to decide
a second has passed: ticks stop while the compositor copies with interrupts
masked, so on the Pi they fall behind real time.

Sample output (headless QEMU, right after boot):

```
DBG 1s: refreshes=62
  pid 1 "term"  copies=1 held=1
  pid 2 "basic" copies=1 held=1
  pid 3 "clock" copies=2 held=2
  pid 4 "pong"  copies=61 held=0
```

## How to re-enable

1. Add the includes back to the top of `compositor.c`:

   ```c
   #include "printf.h"
   #include "debug.h"
   ```

2. Add the statics next to `owed` / `border_owed` / `last_fg`:

   ```c
   // --- Quest 5 Part A: compositor profiling (compiles to nothing at DEBUG_LVL=0) ---
   // Decide a second has passed with the hardware counter, NOT timer_ticks(): ticks
   // stop while we copy with interrupts masked, so on the Pi they fall behind real time.
   static inline uint64 dbg_cntpct(void){ uint64 c; asm volatile("mrs %0, cntpct_el0":"=r"(c)); return c; }
   static inline uint64 dbg_cntfrq(void){ uint64 f; asm volatile("mrs %0, cntfrq_el0":"=r"(f)); return f; }
   static uint64 dbg_sec_start = 0;   // CNTPCT at the start of the current 1-second window
   static uint64 dbg_refreshes = 0;   // refreshes composited this second
   static uint64 dbg_copies[NPROC];   // fresh canvas copies per window this second
   static uint64 dbg_held[NPROC];     // per window this second: 2nd copies, taken from the visible half
   ```

3. Put the three counters back in `compositor_tick`:

   - Right after `uint32 back = fb_backbuffer_y();`:

     ```c
     DEBUG(dbg_refreshes++);
     ```

   - Right after `owed[i] = 1;` (the first / fresh-canvas copy):

     ```c
     DEBUG(dbg_copies[i]++);
     ```

   - Right after `owed[i] = 0;` (the second / hold-back copy):

     ```c
     DEBUG(dbg_held[i]++);
     ```

4. Put the per-second report back, right after `wakeup(fb_get());`:

   ```c
   // Once a real second has elapsed, report refreshes and per-window copies.
   DEBUG({
       uint64 now = dbg_cntpct();
       if (dbg_sec_start == 0) dbg_sec_start = now;
       if (now - dbg_sec_start >= dbg_cntfrq()){
           printf("DBG 1s: refreshes=%d\r\n", (int)dbg_refreshes);
           for (int i = 0; i < NPROC; i++){
               struct proc* p = &process_table[i];
               if (p->usesScreen)
                   printf("  pid %d \"%s\" copies=%d held=%d\r\n",
                          p->processID, p->procName,
                          (int)dbg_copies[i], (int)dbg_held[i]);
               dbg_copies[i] = 0;
               dbg_held[i] = 0;
           }
           dbg_refreshes = 0;
           dbg_sec_start = now;
       }
   });
   ```
