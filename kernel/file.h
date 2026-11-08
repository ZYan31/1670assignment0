#ifndef _FILE_H
#define _FILE_H

#include "types.h"

enum filetype { FILE_CONSOLE };

struct file {
    enum filetype ft;
    uint64 ref_ct;
};

extern struct file console;  // the single global UART-console file object

#endif  // _FILE_H