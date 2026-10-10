#include "ringbuf.h"

void rb_init(struct ringbuf* rb){
    rb -> read_p = 0;
    rb -> write_p = 0;
    rb -> count = 0;
    for (int i = 0; i < R_CAP; i++){
        rb -> buffer[i] = '\0';
    }
}

void rb_push(struct ringbuf* rb, char c){
    if (rb -> count == R_CAP){
        rb -> dropped++;
        return;
    }
    rb->buffer[rb->write_p] = c;
    rb->write_p = (rb->write_p + 1) % R_CAP;
    rb->count++;
}

char rb_pop(struct ringbuf* rb){
    char c = rb->buffer[rb->read_p];
    rb->read_p = (rb->read_p + 1) % R_CAP;
    rb->count--;
    return c;
}

bool rb_isEmpty(struct ringbuf* rb){
    return (rb -> count == 0);
}