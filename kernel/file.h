#ifndef _FILE_H
#define _FILE_H

#include "types.h"

enum filetype { FILE_CONSOLE, FILE_PIPE_READ, FILE_PIPE_WRITE };

struct pipe;   // defined in pipe.h; a pipe end's struct file points back at it

struct file {
    enum filetype ft;
    uint64 ref_ct;
    struct pipe* pipe;   // FILE_PIPE only: the pipe this end belongs to
    bool is_write;       // FILE_PIPE only: true = write end, false = read end
};

extern struct file console;  // the single global UART-console file object

#endif  // _FILE_H