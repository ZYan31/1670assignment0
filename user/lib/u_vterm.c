#include "u_vterm.h"
#include "u_fb.h"
#include "u_string.h"

void term_init(term_t *term, struct canvas* s){
    term->canvas = s;
    term->scale  = 1;
    term->cols   = s->width  / (8 * term->scale);   // whole cells that fit across
    term->rows   = s->height / (8 * term->scale);   // ...and down
    term->col    = 0;
    term->row    = 0;
    term->bgc    = COLOR_BLACK;   // 0xFF000000
    term->tc     = COLOR_WHITE;   // 0xFFFFFFFF
    fb_fill_rect(term->canvas, 0, 0, term->canvas->width, term->canvas->height, term->bgc);
}

// Scroll the whole terminal up by one text row: copy every pixel row up by one
// cell height (each row inherits the row one cell below it), then clear the
// freed bottom row to the background. This is the "memcpy rows up" from the spec.
static void term_scroll(term_t *term){
    uint32 cell = 8 * term->scale;
    struct canvas* c = term->canvas;
    for (uint32 y = 0; y + cell < c->height; y++){          // dst row y <- src row y+cell
        uint32* dst = c->pixels + (uint64)y * c->stride;
        uint32* src = c->pixels + (uint64)(y + cell) * c->stride;
        // for (uint32 x = 0; x < c->width; x++) dst[x] = src[x];   // Quest 5 B: word-at-a-time
        memcpy(dst, src, (uint64)c->width * sizeof(uint32));   // rows are cell*stride apart -> no overlap
    }
    fb_fill_rect(c, 0, c->height - cell, c->width, cell, term->bgc);  // clear last row
    c->changed = 1;   // the row-copy above wrote pixels directly (memcpy), so mark it ourselves
}

// Move to the start of the next row, scrolling up if we've run past the last line.
static void term_newline(term_t *term){
    term->col = 0;
    term->row++;
    if (term->row >= term->rows){     // off the bottom: scroll, stay on the last row
        term_scroll(term);
        term->row = term->rows - 1;
    }
}

void term_putc(term_t *term, char c){
    if (c == '\n'){ term_newline(term); return; }    // newline: next row (may scroll)
    if (c == '\r'){ term->col = 0; return; }          // carriage return: back to col 0
    if (term->col == term->cols) term_newline(term);  // row full: wrap (may scroll)
    // Cursor is in character cells; draw_char wants pixels. One cell is
    // 8*scale px (the 8x8 glyph, scaled), so (col,row) -> (col*cell, row*cell).
    uint32 cell = 8 * term->scale;
    draw_char(term->canvas, term->col * cell, term->row * cell,
              term->tc, (unsigned char)c, term->scale);
    term->col++;                     // cursor now points at the next cell
}

void term_printf(term_t *term, const char* fmt, ...){
    char out[256];
    va_list args;                 // same trick as u_common.c's printf:
    va_start(args, fmt);          //   grab the args that follow `fmt`,
    vsprintf(out, fmt, args);
    va_end(args);
    for (int i = 0; out[i] != '\0'; i++){
        term_putc(term, out[i]);
    }
}

// Clear the whole screen and park the cursor back at the top-left.
void term_clear(term_t *term){
    fb_fill_rect(term->canvas, 0, 0, term->canvas->width, term->canvas->height, term->bgc);
    term->col = 0;
    term->row = 0;
}


