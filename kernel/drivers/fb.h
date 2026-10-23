#ifndef _DRIVERS_FB
#define _DRIVERS_FB

#include "memlayout.h"
#include "mbox.h"
#define FB_WIDTH 640
#define FB_HEIGHT 480
#define FB_DEPTH 32

int fb_init(void);
struct fb_info* fb_get(void);   // the kernel's private screen description

// Double buffering (Quest 4 Part C). The virtual framebuffer is two screens
// tall; one half is scanned out, the compositor draws into the other.
void   virt_call(void);          // present the hidden half (flip the buffers)
uint32 fb_backbuffer_y(void);    // row offset of the hidden half: draw here

struct fb_info {
 uint32  width;    // visible width in pixels
 uint32  height;   // visible height in pixels
 uint32  pitch;    // bytes from the start of one row to the start of the next
 uint32  pitch_w;  // the same thing in units of uint32 (pitch / 4), for convenience
 uint32  depth;    // bits per pixel
 uint32* pixels;   // where to draw. NULL means "no framebuffer"
};

struct canvas {
  void* pixels;
  uint32  width;
  uint32  height;
  uint32  stride;
  volatile uint32 changed;
};
#endif