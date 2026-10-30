// _start: where every user program begins. The Makefile links each program
// with `-e _start`, so this is the first instruction the kernel's `eret`
// lands on. It calls the program's main() and hands main's return value to
// exit(), so returning from main() ends a process just like calling exit().
//
// This file declares main() and exit() itself rather than including
// u_common.h or u_syscalls.h, so that it compiles whatever your headers
// currently say. It deliberately does *not* declare exit() [[noreturn]]: if
// your exit() returns (as it does before you implement it as a system call),
// the loop below catches the process instead of letting it run off into
// whatever code follows _start.

int main(void);
void exit(int status);

void _start(void) {
  exit(main());

  for (;;) {
    // exit() returned. Spin forever.
  }
}
