#include "drivers/mbox.h"

#include "types.h"
#include "utils.h"

// Send one property-channel message to the GPU and wait for the reply.
//
// This is a polling driver, like your Project 1 UART: there is no interrupt
// involved and we simply spin until the hardware is ready. That is fine here
// because mailbox calls are rare (a handful at boot, and one per frame in
// Quest 5) and the GPU answers quickly.
int mbox_call(volatile uint32* buf) {
  // Build the message word: the buffer's address in the top 28 bits, the
  // channel number in the bottom 4. This is why the buffer has to be 16-byte
  // aligned -- otherwise its low bits would collide with the channel.
  uint32 msg = ((uint32)(uint64)buf & ~0xFu) | (MBOX_CH_PROP & 0xFu);

  // Wait until the GPU has room for a request.
  while (mmio_read32(MBOX_STATUS) & MBOX_FULL) {
    __asm__ volatile("nop");
  }

  mmio_write32(MBOX_WRITE, msg);

  // Wait for *our* reply. The mailbox is shared between channels, so a read
  // can return somebody else's message; the reply we want is the one whose
  // word matches the one we sent.
  while (1) {
    while (mmio_read32(MBOX_STATUS) & MBOX_EMPTY) {
      __asm__ volatile("nop");
    }
    if (mmio_read32(MBOX_READ) == msg) {
      // The GPU has written its answers back into our buffer in place, and
      // put a status code in buf[1].
      return buf[1] == MBOX_SUCCESS;
    }
  }
}
