#include "uaccess.h"

int user_range_ok(const void* uaddr, uint64 n) {
    // pointer boundaries: current_
    uint64 high= (uint64)current_process->stack;
    uint64 low = high - PROC_SIZE;
    uint64 start = (uint64)uaddr;
    uint64 end = start + n;
    if(end < start) {return -1;}
    if ( start > low  && end <= high){return 0;}
    return -1;
}

int copy_from_user(void* kdst, const void* usrc, uint64 n){
    if(!user_range_ok(usrc, n)){
        memcpy(kdst, usrc, n);
        return 0;
    }
    return -1;
};

int copy_to_user(void* udst, const void* ksrc, uint64 n){
    if(!user_range_ok(udst, n)){
        memcpy(udst, ksrc, n);
        return 0;
    }
    return -1;
};