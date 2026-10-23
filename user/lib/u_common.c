#include "u_common.h"

#include "u_klib.h"
#include "u_types.h"

void printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  // calls the function which address is contained in F_VPRINTF
  (*(void (**)(const char*, va_list))F_VPRINTF)(fmt, args);

  va_end(args);
};

void yield(){
  (*(void (**)(void))F_YIELD)();
}

char getc(){
  return (*(char (**)(void))F_GETC)();
}

int hasc(){
  return (*(int (**)(void))F_HASC)();
}

struct canvas* get_canvas(void){
  return (*(struct canvas* (**)(void))F_GET_CANVAS)();
}

void wait_for_vsync(){
  (*(void (**)(void))F_VSYNC)();
}

void exit() {
  return;
}

// Follow the F_UPTIME pointer the kernel installed and call through it.
uint64 uptime_ms() {
  return (*(uint64 (**)(void))F_UPTIME)();
}

// Same trick for F_PS: the kernel reads slot `index` and fills in *out.
int get_proc_info(uint32 index, struct proc_info* out) {
  return (*(int (**)(uint32, struct proc_info*))F_PS)(index, out);
}
