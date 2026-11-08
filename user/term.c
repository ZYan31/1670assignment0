#include "u_vterm.h"
#include "u_fb.h"
#include "u_string.h"
#include "u_common.h"   // yield()

// The text console (term_init / term_putc / term_printf / term_clear) lives in
// u_vterm.c now, so term, basic and clock all share it. See u_vterm.h.

// ---- terminal application: prompt, line editing, and a few commands --------

#define LINE_MAX 128

// term_clear moved to u_vterm.c (shared); run_command uses it via u_vterm.h.

// Erase the last glyph: step the cursor back one cell (crossing a row boundary
// if needed) and repaint that cell with the background color.
static void term_backspace(term_t *term){
    if (term->col > 0){
        term->col--;
    } else if (term->row > 0){
        term->row--;
        term->col = term->cols - 1;
    }
    uint32 cell = 8 * term->scale;
    fb_fill_rect(term->canvas, term->col * cell, term->row * cell, cell, cell, term->bgc);
}

// Run one finished command line.
static void run_command(term_t *term, char *line){
    // Split the first word (the command) from the rest (its arguments).
    char *rest = line;
    while (*rest && *rest != ' ') rest++;
    if (*rest){ *rest = '\0'; rest++; }   // cut the command word; rest -> arguments

    if (strcmp(line, "help") == 0){
        term_printf(term, "commands: help  clear  echo <text>  ps  uptime\n");
    } else if (strcmp(line, "clear") == 0){
        term_clear(term);
    } else if (strcmp(line, "echo") == 0){
        term_printf(term, "%s\n", rest);
    } else if (strcmp(line, "ps") == 0){
        struct proc_info info[16];            // 16 == kernel NPROC
        int n = process_info(info, 16);       // one syscall fills the whole array
        term_printf(term, "PID  NAME       STATE\n");
        for (int i = 0; i < n; i++)
            term_printf(term, "%d    %s    %s\n", info[i].pid, info[i].name, info[i].state);
    } else if (strcmp(line, "uptime") == 0){
        uint64 ms = ticks();
        term_printf(term, "up %lu.%lu s\n", ms / 1000, (ms % 1000) / 100);
    } else if (line[0] != '\0'){
        term_printf(term, "unknown command: %s\n", line);
    }
}

int main(void){
    term_t term;
    struct canvas *c = init_canvas();   // claim a window + canvas, or NULL if none free
    if (!c) {
        printf("term: no canvas, nothing to draw\r\n");
        for (;;) yield();
    }
    term_init(&term, c);

    char line[LINE_MAX];
    uint32 len = 0;
    term_printf(&term, "> ");

    for (;;){
        char c = getc();                       // blocks until the next keystroke

        if (c == '\n' || c == '\r'){           // Enter: finish and run the line
            term_putc(&term, '\n');
            line[len] = '\0';
            run_command(&term, line);
            len = 0;
            term_printf(&term, "> ");
        } else if (c == 0x08 || c == 0x7F){    // Backspace (BS) or Delete (DEL)
            if (len > 0){
                len--;
                term_backspace(&term);
            }
        } else {                               // ordinary key: echo it and buffer it
            if (len < LINE_MAX - 1){
                line[len++] = c;
                term_putc(&term, c);
            }
        }
    }
}
