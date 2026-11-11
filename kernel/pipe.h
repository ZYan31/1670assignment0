#ifndef _PIPE_H
#define _PIPE_H

#include "ringbuf.h"
#include "file.h"

#define NPIPE 8   // how many pipes can be open at once, system-wide

// A pipe is a bounded byte buffer with two ends. Each end is a struct file,
// embedded here so its ref_ct already counts that end's open fds: wr.ref_ct
// is the number of writers, rd.ref_ct the number of readers.
struct pipe {
    ringbuf_t buf;     // bytes in flight (R_CAP capacity)
    struct file rd;    // read end
    struct file wr;    // write end
    bool used;         // pool slot in use?
};

// Grab a free pipe from the pool, init its buffer, and wire up its two ends.
// Returns the pipe, or nullptr if all NPIPE are taken.
struct pipe* pipe_alloc(void);

#endif  // _PIPE_H
