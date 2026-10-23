#ifndef _LIB_U_FB_H
#define _LIB_U_FB_H

#include "u_types.h"

// Quest 4: the framebuffer is no longer published to user space. Programs get
// their own canvas from get_canvas() instead, so FB_INFO_BASE and struct
// fb_info are gone from here -- only the kernel knows about the real screen.
// #define FB_INFO_BASE 0x10000000
// struct fb_info { uint32 width, height, pitch, pitch_w, depth; uint32* pixels; };

// A drawable surface: a pixel pointer, its size, and the row stride in pixels
// (so the pixel at (x, y) is pixels[y * stride + x]).
struct canvas {
  uint32* pixels;
  uint32  width;
  uint32  height;
  uint32  stride;
  volatile uint32 changed;
};

// Colors are 0xAABBGGRR (low byte red).
#define COLOR_BLACK 0xFF000000
#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_RED   0xFF0000FF
#define COLOR_GREEN 0xFF00FF00
#define COLOR_BLUE  0xFFFF0000

void fb_set_pixel(struct canvas* c, uint32 x, uint32 y, uint32 color);
void fb_fill_rect(struct canvas* c, uint32 x, uint32 y, uint32 w, uint32 h, uint32 color);
void draw_char(struct canvas* c, uint32 x, uint32 y, uint32 color, unsigned char glyph, uint32 scale);
void draw_string(struct canvas* c, int x, int y, uint32 color, const char* s, uint32 scale);
struct canvas* get_canvas(void);
#endif  // _LIB_U_FB_H
