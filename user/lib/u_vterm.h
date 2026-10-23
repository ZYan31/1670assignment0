#ifndef _LIB_U_VTERM_H
#define _LIB_U_VTERM_H

#include "u_types.h"
#include "u_fb.h"
typedef struct{
    struct canvas* canvas;
    uint32 col, row; // the pointer
    uint32 cols, rows; //width of screen
    uint32 bgc, tc; //background color, text color
    uint32 scale; //scale of text
} term_t;

// A reusable text console that draws on a canvas. term.c and basic.c (and the
// clock) all share these: get your canvas from get_canvas(), hand it to
// term_init, then push characters with term_putc / term_printf.
void term_init(term_t *term, struct canvas* s);
void term_putc(term_t *term, char c);
void term_printf(term_t *term, const char* fmt, ...);
void term_clear(term_t *term);

#endif