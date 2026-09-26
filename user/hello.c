#include "u_common.h"

/* hello.c - print a message, give up the CPU, and do it again, forever.
 *
 * This program is deliberately *cooperative*: it hands the CPU back to the
 * kernel on every iteration, so that other processes get a turn. Compare
 * primecheck.c, which never yields at all.
 *
 * The two messages let you see where a process was interrupted: if you ever
 * see "before yield" from this process followed by output from a *different*
 * process, you are watching a context switch happen.
 *
 * NOTE: the yield() call below is commented out, because your kernel does not
 * expose a yield() function yet. Uncomment it in Quest 3, once you have added
 * the user-side yield() wrapper to u_common.h.
 */

int main(void) {
  for (;;) {
    printf("Process hello: before yield\r\n");
    // yield();
    printf("Process hello: after yield\r\n");
  }

  exit();
}
