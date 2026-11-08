#ifndef _UACCESS_H
#define _UACCESS_H

#include "types.h"
#include "proc.h"
#include "utils.h"

int user_range_ok(const void* uaddr, uint64 n);
int copy_from_user(void* kdst, const void* usrc, uint64 n);
int copy_to_user(void* udst, const void* ksrc, uint64 n);

#endif  // _UACCESS_H