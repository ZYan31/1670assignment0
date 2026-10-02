#ifndef _SCHEDULING_H
#define _SCHEDULING_H

#include "proc.h"      // struct proc
#include "context.h"   // context_t

struct proc* pick_next(void);
void resume(struct proc* p);
void scheduler(void);
void handle_yield(context_t* ctx);

#endif  // _SCHEDULING_H
