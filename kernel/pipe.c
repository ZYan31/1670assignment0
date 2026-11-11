#include "pipe.h"

static struct pipe pipes[NPIPE];

struct pipe* pipe_alloc(void){
    for (int i = 0; i < NPIPE; i++){
        if (!pipes[i].used){
            struct pipe* p = &pipes[i];
            p->used = true;
            rb_init(&p->buf);
            // Both ends point back at this pipe; ref_ct starts at 0 and the
            // caller bumps it as it installs each end in an fd slot.
            p->rd = (struct file){ .ft = FILE_PIPE_READ,  .ref_ct = 0, .pipe = p, .is_write = false };
            p->wr = (struct file){ .ft = FILE_PIPE_WRITE, .ref_ct = 0, .pipe = p, .is_write = true  };
            return p;
        }
    }
    return nullptr;   // all pipes in use
}
