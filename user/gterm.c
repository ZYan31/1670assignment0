// gterm.c - the graphical terminal EMULATOR (CS 1670 / 2670, Quest 5).
//
// This is term.c with the shell removed: it does NO command parsing and NO
// line editing. Its whole job is to be a pipe-pump between the keyboard and a
// separate shell process (user/sh.c), drawing the shell's output in a window.
// Think xterm (this file) vs. bash (sh.c).
//
// Full flow (handout, Project 4 Quest 5):
//   init_canvas()  -> our window + canvas        (drawing setup)
//   open_console() -> OUR OWN keyboard fd        (syscall 51; so our window
//                     can hold the focus instead of reading keys second-hand
//                     through the terminal that started us)
//   close(0)       -> drop the stdin we inherited
//   pipe(in); pipe(out)                          -> two pipes for the shell
//   spawn("sh", in[0], out[1])                   -> shell: reads in[0], writes out[1]
//   close(in[0]); close(out[1])                  -> we don't own the shell's ends
//   loop (the pump):
//     keyboard byte            -> write into in[1]   (keyboard -> shell stdin)
//     byte from out[0]         -> emu_draw()          (shell stdout -> canvas)
//     use peek() so we never block on one pipe while the other has data;
//     yield() when neither does; peek(out[0]) == -1 (EOF) => shell exited:
//     wait(sh) and exit.
//
// AI-use note (per the course policy for this task): the EMULATOR DRAWING below
// is AI-allowed. The MECHANISM in main() (open_console, pipes, spawn, the pump)
// is the part you implement yourself -- it is left as a labeled stub.

#include "u_common.h"   // read/write/pipe/close/spawn/wait/peek/yield/exit, printf
#include "u_fb.h"       // struct canvas
#include "u_vterm.h"    // term_t, term_init/term_putc/term_clear  (the text console)
#include "u_string.h"   // strlen

enum { E_NORMAL, E_ESC, E_CSI };   // escape-sequence state: text, saw ESC, saw ESC[

typedef struct {
    term_t vt;      // the canvas text console we draw into
    int    state;   // position in an ESC [ ... sequence
} emulator_t;

static void emu_init(emulator_t* e, struct canvas* c){
    term_init(&e->vt, c);
    e->state = E_NORMAL;
}

// Draw one byte of shell output onto the canvas.
static void emu_putc(emulator_t* e, char ch){
    switch (e->state){
    case E_NORMAL:
        if (ch == 0x1B){ e->state = E_ESC; return; }   // ESC: an escape begins
        term_putc(&e->vt, ch);                         // ordinary character: draw it
        return;
    case E_ESC:
        e->state = (ch == '[') ? E_CSI : E_NORMAL;     // only ESC[ ... interests us
        return;
    case E_CSI:
        // Collect (and ignore) parameter bytes '0'-'9' and ';' until the final
        // byte (0x40-0x7E) that names the command.
        if (ch >= 0x40 && ch <= 0x7E){
            if (ch == 'J')      term_clear(&e->vt);              // ESC [ 2 J : clear
            else if (ch == 'H'){ e->vt.col = 0; e->vt.row = 0; } // ESC [ H   : home
            // any other final byte: ignore the whole sequence
            e->state = E_NORMAL;
        }
        return;
    }
}

static void emu_draw(emulator_t* e, const char* buf, int n){
    for (int i = 0; i < n; i++) emu_putc(e, buf[i]);
}

int main(void){
    struct canvas* c = init_canvas();
    if (!c){ printf("gterm: no canvas, nothing to draw\r\n"); for(;;) yield(); }
    emulator_t emu;
    emu_init(&emu, c);
    //what needs to be done.
    int in[2], out[2], n, g, o, kbd;
    kbd = open_console();
    close(0);
    pipe(in);
    pipe(out);
    char buf[128];
    int sh_pid = spawn("sh", in[0], out[1]);
    close(in[0]);
    close(out[1]);
    while(true){
        n = peek(kbd);
        if (n <= 0) {yield();}
        if (n > 0) {g = read(kbd, buf, sizeof(buf)); write(in[1], buf, g);}
        o = peek(out[0]);
        if (o < 0) {wait(sh_pid); exit(0);}
        if (o > 0) {g = read(out[0], buf, sizeof(buf)); emu_draw(&emu, buf, g);}
    }
}
