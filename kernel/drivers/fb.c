#include "fb.h"
#include "printf.h"
#include "utils.h"

// Must be 16-byte aligned: the low 4 bits of its address encode the channel.
static volatile uint32 __attribute__((aligned(16))) mbox_buf[36];  // 35 used

// The kernel's private description of the screen. User programs no longer get
// a struct fb_info at a fixed address; only the kernel sees this, via fb_get().
static struct fb_info g_fb;
struct fb_info* fb_get(void) { return &g_fb; }

// Which half of the virtual buffer the GPU is currently scanning out: 0 = top,
// FB_HEIGHT = bottom. We track this ourselves; the mailbox only obeys.
static uint32 visible_y = 0;

// The hidden half -- the one not on screen, where the compositor draws.
uint32 fb_backbuffer_y(void) { return visible_y == 0 ? FB_HEIGHT : 0; }

// Present the hidden half: tell the GPU to scan out starting at that half, then
// record it as the new visible one. A single-tag SET_VIRT_OFFS message.
void virt_call(void) {
  uint32 hidden = fb_backbuffer_y();
  static volatile uint32 __attribute__((aligned(16))) msg[8];
  msg[0] = 8 * 4;                    // total size of this buffer, in bytes
  msg[1] = MBOX_REQUEST;             // 0 == "this is a request"
  msg[2] = MBOX_TAG_SET_VIRT_OFFS;   // pan the display
  msg[3] = 8;                        // value buffer is 2 x uint32
  msg[4] = 0;                        // request/response code
  msg[5] = 0;                        // x offset
  msg[6] = hidden;                   // y offset -> make the hidden half visible
  msg[7] = MBOX_TAG_LAST;            // end of tag list
  mbox_call(msg);
  visible_y = hidden;                // that half is live now
}

int fb_init(void) {
  mbox_buf[0]  = 35 * 4;               // total size of this buffer, in bytes
  mbox_buf[1]  = MBOX_REQUEST;         // 0 == "this is a request"

  mbox_buf[2]  = MBOX_TAG_SET_PHYS_WH; // set the display resolution
  mbox_buf[3]  = 8;                    // value buffer is 2 x uint32
  mbox_buf[4]  = 0;
  mbox_buf[5]  = FB_WIDTH;
  mbox_buf[6]  = FB_HEIGHT;

  mbox_buf[7]  = MBOX_TAG_SET_VIRT_WH; // set the in-memory buffer size
  mbox_buf[8]  = 8;                    // Quest 4: twice as tall as the display,
  mbox_buf[9]  = 0;                    // so we have a front and a back buffer.
  mbox_buf[10] = FB_WIDTH;
  mbox_buf[11] = FB_HEIGHT * 2;

  mbox_buf[12] = MBOX_TAG_SET_VIRT_OFFS;  // no panning
  mbox_buf[13] = 8;
  mbox_buf[14] = 0;
  mbox_buf[15] = 0;                    // x offset
  mbox_buf[16] = 0;                    // y offset

  mbox_buf[17] = MBOX_TAG_SET_DEPTH;
  mbox_buf[18] = 4;
  mbox_buf[19] = 0;
  mbox_buf[20] = FB_DEPTH;             // 32 bits per pixel

  mbox_buf[21] = MBOX_TAG_SET_PIX_ORDER;
  mbox_buf[22] = 4;
  mbox_buf[23] = 0;
  mbox_buf[24] = 1;                    // 1 == RGB

  mbox_buf[25] = MBOX_TAG_GET_FB;      // allocate the framebuffer
  mbox_buf[26] = 8;
  mbox_buf[27] = 0;
  mbox_buf[28] = 4096;                 // request 4 KiB alignment;
  mbox_buf[29] = 0;                    // ...the GPU replaces [28] with the bus address
                                       //    and [29] with the size in bytes

  mbox_buf[30] = MBOX_TAG_GET_PITCH;   // ask how many bytes a row takes
  mbox_buf[31] = 4;
  mbox_buf[32] = 0;
  mbox_buf[33] = 0;                    // ...the GPU replaces [33] with the pitch

  mbox_buf[34] = MBOX_TAG_LAST;        // end of tag list
  struct fb_info* fb = &g_fb;
  if (!mbox_call(mbox_buf)) {
    *fb = (struct fb_info){0};
    // TODO: zero the struct at FB_INFO_BASE (in particular, pixels = NULL)
    //       before returning, so that user programs can tell there is no display.
    return 0;   // the GPU said no
  }
  fb->pixels = (uint32*)(uint64)(0x3FFFFFFF & mbox_buf[28]); //0x3FFFFFFF is exactly top 2 bits = 0.
  fb->width = FB_WIDTH;
  fb->height = FB_HEIGHT;               // VISIBLE height; the buffer is 2x this tall
  fb->depth = FB_DEPTH;
  fb->pitch = mbox_buf[33];
  fb->pitch_w = (uint32)(mbox_buf[33]/4);
  // Paint both halves white ONCE. The gaps between windows never change, so the
  // compositor only repaints canvases and borders each frame -- it never has to
  // wipe a whole half, which is what used to flash white when the flip raced the
  // display. 0xFF bytes == 0xFFFFFFFF == opaque white.
  memset(fb->pixels, 0xFF, (uint64)fb->pitch * FB_HEIGHT * 2);
  printf("fb: %dx%d depth %d pitch %d pixels %p size %d\r\n", 
    fb->width, fb->height, fb->depth, fb->pitch, fb->pixels, mbox_buf[29]);
  return 1;
  // TODO: mbox_buf[28] is a *bus* address; mbox_buf[33] is the pitch.
  //       Turn them into a struct fb_info at FB_INFO_BASE.
}