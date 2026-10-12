#include "u_common.h"

/* keytest.c - echo keystrokes one at a time.
 *
 * Prints a prompt, blocks in getc() until a byte is available, then prints
 * the character it got, forever. Exercises the console input path end to end:
 * UART IRQ -> console_handle_input -> input_buf -> getc() (via trampoline).
 */

int main(void) {
  for (;;) {
    printf("key> ");
    char c = getc();
    printf("got '%c'\r\n", c);
  }

  exit();
}
