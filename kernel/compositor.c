#include "compositor.h"
#include "console.h"   // foreground_pid: which window has the keyboard focus

#define BORDER_NORMAL 0xFFFFFFFFu   // white
#define BORDER_FOCUS  0xFF00FF00u   // bright green: the focused window

static uint32 owed[NPROC];         // framebuffer halves this window still owes a copy to (0..2)
static int    border_owed = 0;     // framebuffer halves still owed a border redraw after a focus move
static uint32 last_fg = 0xFFFFFFFFu;  // foreground_pid last seen, so we can notice focus moving

// Fill a w x h rectangle of the back buffer at (x, y) with a solid color.
static void fill(struct fb_info* fb, uint32 back, uint32 x, uint32 y,
                 uint32 w, uint32 h, uint32 color){
    for (uint32 j = 0; j < h; j++){
        uint32* row = fb->pixels + (back + y + j)*fb->pitch_w + x;
        for (uint32 i = 0; i < w; i++) row[i] = color;
    }
}

// A 2px border drawn in the gap just outside the window rectangle.
static void draw_border(struct fb_info* fb, uint32 back, struct rect* w, uint32 color){
    fill(fb, back, w->x - 2, w->y - 2, w->w + 4, 2, color);    // top
    fill(fb, back, w->x - 2, w->y + w->h, w->w + 4, 2, color); // bottom
    fill(fb, back, w->x - 2, w->y, 2, w->h, color);            // left
    fill(fb, back, w->x + w->w, w->y, 2, w->h, color);         // right
}

void compositor_tick(){
    if (timer_ticks() % REFRESH_TICKS == 0) {
        struct fb_info* fb = fb_get();
        uint32 back = fb_backbuffer_y();   // the hidden half we draw this frame

        // A focus move owes both framebuffer halves a border redraw.
        if ((uint32)foreground_pid != last_fg){ border_owed = 2; last_fg = (uint32)foreground_pid; }

        // Copy each window's canvas onto the screen at the window's position.
        for (int i = 0; i < NPROC; i++){
            struct proc* proxy = &process_table[i];
            struct canvas *c = proxy->proc_canvas;
            uint32* src = (uint32*)proxy->canvas_pixels;

            // A completed new frame owes a copy to BOTH halves, so arm the counter to 2.
            if(proxy->usesScreen && c->changed){ owed[i] = 2; c->changed = 0; }

            // First copy: the finished frame is in the canvas.
            if(proxy->usesScreen && owed[i] == 2){
                // pitch_w = screen row stride; stride = canvas row stride.
                for (uint32 n = 0; n < c->height; n++){
                    memcpy(fb->pixels + (back + proxy->window.y + n)*fb->pitch_w + proxy->window.x,
                        src + n*c->stride,
                        c->width * sizeof(uint32));
                }
                owed[i] = 1;
            }
            // Second copy: that finished frame is now in the visible half (post-flip),
            // so copy it from there -- the same source your Quest 4 hold-back used.
            else if(proxy->usesScreen && owed[i] == 1){
                for (uint32 n = 0; n < c->height; n++){
                    memcpy(fb->pixels + (back + proxy->window.y + n)*fb->pitch_w + proxy->window.x,
                        fb->pixels + (FB_HEIGHT - back + proxy->window.y + n)*fb->pitch_w + proxy->window.x,
                        c->width * sizeof(uint32));
                }
                owed[i] = 0;
            }

            // Borders: NOT every refresh -- only while a focus move is owed, and into both halves.
            if(proxy->usesScreen && border_owed > 0){
                uint32 color = (proxy->processID == (uint32)foreground_pid)
                            ? BORDER_FOCUS : BORDER_NORMAL;
                draw_border(fb, back, &proxy->window, color);
            }
        }
        if (border_owed > 0) border_owed--;   // one refresh redraws one half's borders
        virt_call();   // present the hidden half
        wakeup(fb_get());   // the frame is up: release everyone in wait_for_vsync()
    }
}
