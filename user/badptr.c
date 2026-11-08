#include "u_common.h"

/* badptr.c - Quest 3 / Part 3 bad-pointer test.
 *
 * Each call below is illegal. The kernel must reject every one with -1, and
 * must neither leak its own memory nor crash. Expected: all four print -1.
 */
int main(void) {
  char buf[16];

  // (1) buf points into the kernel's address space, not this process's slot.
  int64 r1 = write(1, (void*)0x80000, 16);

  // (2) length runs far past the end of the process slot.
  int64 r2 = write(1, buf, 100000000);

  // (3) destination of a read is a kernel address.
  int64 r3 = read(0, (void*)0x80000, 1);

  // (4) fd 5 is not an open file descriptor.
  int64 r4 = write(5, buf, 1);

  printf("badptr: r1=%d r2=%d r3=%d r4=%d (all should be -1)\r\n",
         (int)r1, (int)r2, (int)r3, (int)r4);

  exit(0);
}
