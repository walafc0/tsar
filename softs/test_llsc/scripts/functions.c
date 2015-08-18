
#include "functions.h"

void take_lock(volatile int * plock) {
   volatile int * _plock = plock;
   __asm__ __volatile__ (
      "move $16, %0                   \n"
      "giet_lock_try :                \n"
      "lw   $2,    0($16)             \n"
      "ll   $2,    0($16)             \n"
      "bnez $2,    giet_lock_try      \n"
      "li   $3,    1                  \n"
      "sc   $3,    0($16)             \n"
      "beqz $3,    giet_lock_try      \n"
      "nop                            \n"
      "giet_lock_ok:                  \n"
      "nop                            \n"
      :                                
      :"r" (_plock)                  
      :"$2", "$3", "$4", "$16");
}


void release_lock(volatile int * plock) {
   asm volatile("\tsync\n");
   *plock = 0;
}


