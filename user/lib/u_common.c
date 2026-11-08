#include "u_common.h"

#include "u_klib.h"
#include "u_types.h"
#include "u_string.h"

void printf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len > 0) write(1, buf, (size_t)len);
};

// void yield(){
//   (*(void (**)(void))F_YIELD)();
// }

char getc(){
  char c;
  if (read(0, &c, 1) != 1) return 0;   // stdin (fd 0); blocks for one byte
  return c;
}

int hasc(){
  return peek(0);                      // bytes available on stdin, no blocking
}

// Trampoline wrappers removed -- these are syscalls now:
//   get_canvas()  -> init_canvas()   (svc #7)
//   wait_for_vsync() -> svc #8
//   uptime_ms()   -> ticks()         (svc #1)
//   get_proc_info() -> process_info() (svc #6)

void exit(int status) {
  (void)status;
  for (;;) { yield(); }
}
