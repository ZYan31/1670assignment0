#include "u_fb.h"
#include "font8x8.h"

void fb_set_pixel(struct canvas* c, uint32 x, uint32 y, uint32 color) {
  if (x >= c->width || y >= c->height) return;   // clip to the canvas
  c->pixels[y * c->stride + x] = color;
  c->changed = 1;
}

void fb_fill_rect(struct canvas* c, uint32 x, uint32 y, uint32 w, uint32 h, uint32 color) {
  for (uint32 j = 0; j < h; j++)
    for (uint32 i = 0; i < w; i++)
      fb_set_pixel(c, x + i, y + j, color);
}

void draw_char(struct canvas* c, uint32 x, uint32 y, uint32 color, 
  unsigned char glyph, uint32 scale){
  if (!scale){ scale = 1;}
  const char* todraw = font8x8_basic[glyph];
  for (uint32 j = 0; j < 8*scale; j++)
    for (uint32 i = 0; i < 8*scale; i++)
      if (todraw[j/scale] & (1 << (i/scale)))
        fb_set_pixel(c, x+i, y+j, color);
}

// Draw a string left-to-right; each glyph is 8px wide, +1 gap = 9px advance.
void draw_string(struct canvas* c, int x, int y, uint32 color,
                 const char* s, uint32 scale){
  for (; *s; s++){
    draw_char(c, (uint32)x, (uint32)y, color, (unsigned char)*s, scale);
    x += 9 * (int)scale;
  }
}