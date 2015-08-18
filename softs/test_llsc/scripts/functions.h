#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_


void take_lock(volatile int * plock);
void release_lock(volatile int * plock);

#endif
