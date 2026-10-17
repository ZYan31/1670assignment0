#ifndef _DRIVERS_MBOX_H
#define _DRIVERS_MBOX_H

// VideoCore GPU mailbox interface for the Raspberry Pi 3B (BCM2837).
//
// The display hardware belongs to the GPU, not to the ARM cores, so there is
// no "framebuffer base address" register you can write. Instead you *ask* the
// GPU's firmware for a framebuffer, by sending it a message.
//
// This file and mbox.c are provided in full: the transport is fiddly, and the
// tag numbers below are a table copied out of a wiki page rather than
// something you can reason your way to. What you write in Quest 3 is the part
// that decides what to ask for and what to do with the answer.
//
// The protocol is documented here (there is no vendor datasheet for it):
// https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface

#include "memlayout.h"
#include "types.h"

// ---------------------------------------------------------------------------
// The transport: three MMIO registers.
//
// Mailbox 0 is the direction GPU -> ARM (we read replies from it).
// Mailbox 1 is the direction ARM -> GPU (we write requests to it).
// ---------------------------------------------------------------------------
#define MBOX_BASE (PERIPHERALS_BASE + 0xB880)
#define MBOX_READ (MBOX_BASE + 0x00)    // read a reply from the GPU
#define MBOX_STATUS (MBOX_BASE + 0x18)  // can we write / is there anything to read?
#define MBOX_WRITE (MBOX_BASE + 0x20)   // write a request to the GPU

// Status register flags.
#define MBOX_FULL 0x80000000u   // set: the mailbox cannot accept a write
#define MBOX_EMPTY 0x40000000u  // set: there is nothing to read

// A mailbox message is one 32-bit word: the low 4 bits are a channel number
// and the high 28 bits are the physical address of a request buffer. Because
// the low four bits are stolen for the channel, that buffer must be 16-byte
// aligned.
//
// Channel 8 is the "property" channel, the general-purpose ARM <-> GPU
// request channel, and the only one we use.
#define MBOX_CH_PROP 8

// ---------------------------------------------------------------------------
// The payload: a property request buffer.
//
//   buf[0]  total size of the whole buffer, in BYTES (not words, not tags)
//   buf[1]  request/response code: MBOX_REQUEST going out,
//           MBOX_SUCCESS or MBOX_ERROR coming back
//   buf[2]  first tag id           -+
//   buf[3]  size of its value buffer, in bytes, big enough for the RESPONSE
//   buf[4]  request/response code   | one tag
//   buf[5.] its value words        -+   (the GPU overwrites these in place)
//   ...     more tags ...
//   buf[n]  MBOX_TAG_LAST, terminating the list
// ---------------------------------------------------------------------------
#define MBOX_REQUEST 0x00000000
#define MBOX_SUCCESS 0x80000000
#define MBOX_ERROR 0x80000001

// Property tags. Each one is a small request the GPU knows how to answer;
// a single message may carry as many as you like, and they are processed in
// order, which is how one round trip can configure and allocate a
// framebuffer in one go.
#define MBOX_TAG_SET_PHYS_WH 0x00048003    // set physical (on-screen) width/height
#define MBOX_TAG_SET_VIRT_WH 0x00048004    // set virtual (in-memory) width/height
#define MBOX_TAG_SET_VIRT_OFFS 0x00048009  // set virtual offset (panning)
#define MBOX_TAG_SET_DEPTH 0x00048005      // set color depth, in bits per pixel
#define MBOX_TAG_SET_PIX_ORDER 0x00048006  // set pixel order (0 = BGR, 1 = RGB)
#define MBOX_TAG_GET_FB 0x00040001         // allocate the framebuffer
#define MBOX_TAG_GET_PITCH 0x00040008      // get bytes per scanline
#define MBOX_TAG_LAST 0x00000000           // end of the tag list

// The address the GPU hands back is a VideoCore *bus* address, whose top two
// bits are an alias selector that means nothing to an ARM core. Mask them off
// to get a physical address you can actually dereference.
#define MBOX_BUS_TO_PHYS(addr) ((addr) & 0x3FFFFFFFu)

// Send a property-channel message to the GPU and wait for its reply.
//
// `buf` must be 16-byte aligned (declare it with
// `__attribute__((aligned(16)))`) and laid out as described above. The GPU
// writes its response into the same buffer.
//
// Returns 1 if the GPU reported success, 0 otherwise.
int mbox_call(volatile uint32* buf);

#endif  // _DRIVERS_MBOX_H
