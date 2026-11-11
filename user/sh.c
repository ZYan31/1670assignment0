#include "u_common.h"   // printf() -> fd 1, getc() -> fd 0, struct proc_info, syscalls
#include "u_string.h"   // strcmp

// The shell: all the command logic and line editing that used to live in
// user/term.c, with NO drawing. It reads keystrokes from fd 0 one at a time,
// echoes them to fd 1 (handling backspace itself), and writes all command
// output to fd 1. Programs are started with spawn(), inheriting the shell's
// own fds 0 and 1. A terminal emulator supplies those two descriptors.

#define LINE_MAX 128

// Parse a small non-negative integer from a string (for `wait <pid>`).
static int sh_atoi(const char *s){
    while (*s == ' ') s++;
    int v = 0;
    while (*s >= '0' && *s <= '9'){ v = v * 10 + (*s - '0'); s++; }
    return v;
}

// Does the argument tail mean "run in background"? True for a lone '&'.
static int is_background(const char *s){
    while (*s == ' ') s++;
    return (*s == '&');
}

// Spawn `name` with the shell's OWN fds 0 and 1, so the child shares this
// shell's input and output. Background: print the PID and return. Foreground:
// wait for it and print its exit status.
static void launch(const char *name, int background){
    int pid = spawn(name, 0, 1);          // 0,1 are THIS shell's fds
    if (pid < 0){ printf("no such program: %s\n", name); return; }
    if (background){
        printf("[%d] %s\n", pid, name);   // started, not waited on
    } else {
        int status = wait(pid);           // block until the child exits
        printf("%s exited with status %d\n", name, status);
    }
}

// Run one finished command line.
static void run_command(char *line){
    // Split the first word (the command) from the rest (its arguments).
    char *rest = line;
    while (*rest && *rest != ' ') rest++;
    if (*rest){ *rest = '\0'; rest++; }   // cut the command word; rest -> arguments

    if (strcmp(line, "help") == 0){
        printf("commands: help  clear  echo <text>  ps  uptime  exit\n");
        printf("          <prog> [&]  run <prog> [&]  wait <pid>\n");
    } else if (strcmp(line, "clear") == 0){
        printf("\033[2J\033[H");          // ANSI: clear screen, cursor home
    } else if (strcmp(line, "echo") == 0){
        printf("%s\n", rest);
    } else if (strcmp(line, "ps") == 0){
        struct proc_info info[16];         // 16 == kernel NPROC
        int n = process_info(info, 16);    // one syscall fills the whole array
        printf("PID  PPID  NAME       STATE\n");
        for (int i = 0; i < n; i++)
            printf("%d    %d     %s    %s\n",
                   info[i].pid, info[i].ppid, info[i].name, info[i].state);
    } else if (strcmp(line, "uptime") == 0){
        uint64 ms = ticks();
        printf("up %lu.%lu s\n", ms / 1000, (ms % 1000) / 100);
    } else if (strcmp(line, "run") == 0){
        // Force-treat the next word as a program, even if it names a built-in.
        char *prog = rest;
        while (*rest && *rest != ' ') rest++;         // split prog from its tail
        if (*rest){ *rest = '\0'; rest++; }           // rest -> maybe "&"
        if (prog[0] == '\0')
            printf("run: need a program name\n");
        else
            launch(prog, is_background(rest));
    } else if (strcmp(line, "wait") == 0){
        int pid = sh_atoi(rest);
        int status = wait(pid);            // block until that child exits
        printf("pid %d exited with status %d\n", pid, status);
    } else if (strcmp(line, "exit") == 0){
        exit(0);                           // end the shell
    } else if (line[0] != '\0'){
        // Not a built-in: the command word is a program name. rest is "" or "&".
        launch(line, is_background(rest));
    }
}

int main(void){
    char line[LINE_MAX];
    uint32 len = 0;
    printf("> ");

    for (;;){
        char c = getc();                       // read fd 0, one byte, blocks

        if (c == '\n' || c == '\r'){           // Enter: finish and run the line
            printf("\n");                      // echo newline to fd 1
            line[len] = '\0';
            run_command(line);
            len = 0;
            printf("> ");
        } else if (c == 0x08 || c == 0x7F){    // Backspace (BS) or Delete (DEL)
            if (len > 0){
                len--;
                printf("\b \b");               // erase the last glyph on fd 1
            }
        } else {                               // ordinary key: echo it and buffer it
            if (len < LINE_MAX - 1){
                line[len++] = c;
                write(1, &c, 1);               // echo the key to fd 1
            }
        }
    }
}
