#include "u_common.h"

/* counter.c - count upwards forever, giving up the CPU after each number.
 *
 * Like hello.c, this program is cooperative. It is useful alongside hello.c
 * because its output carries state: if the counter ever skips a value, or
 * restarts, or repeats one, then your context switch is losing a register.
 *
 * NOTE: the yield() call below is commented out, because your kernel does not
 * expose a yield() function yet. Uncomment it in Quest 3, once you have added
 * the user-side yield() wrapper to u_common.h.
 */

int main(void) {
  unsigned int ctr = 0;

  for (;;) {
    printf("Process counter: %d\r\n", ctr++);
    // yield();
  }

  exit();
}
