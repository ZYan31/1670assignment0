#include "u_common.h"

int main(void) {
  for (;;) {
    yield();
  }
  exit();
}