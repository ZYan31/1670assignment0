#include "types.h"

#ifndef _RINGBUF_H
#define _RINGBUF_H

#define R_CAP 512

typedef struct ringbuf {
    char buffer[R_CAP];
    uint32 read_p; //head
    uint32 write_p; //tail
    uint32 count; //number of chars
    uint32 dropped;
} ringbuf_t;

void rb_init(struct ringbuf* rb);
void rb_push(struct ringbuf* rb, char c);
char rb_pop(struct ringbuf* rb);
bool rb_isEmpty(struct ringbuf* rb);

#endif