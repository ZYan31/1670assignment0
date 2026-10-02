#ifndef _EXCEPTIONS_H
#define _EXCEPTIONS_H

void exception_init(void* table_addr);
extern char exception_vector_table[];

#endif // _EXCEPTIONS_H