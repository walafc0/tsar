
#ifndef _functions_h_
#define _functions_h_

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static int mylog2(size_t v) {
   static const size_t b[] = { 0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000 };
   static const size_t S[] = { 1, 2, 4, 8, 16 };
   int i;
   register size_t r = 0;
   for (i = 4; i >= 0; i--) {
      if (v & b[i]) {
         v >>= S[i];
         r |= S[i];
      }
   }
   return r + 1;
}

/*int randint(int a,int b){
   int range = b - a + 1;
   int length = (int) ceil(log2(range));
   int mask = (int) pow(2,length) - 1;
   printf("mask : %d\n",mask);
   int res;
   do {
      res = rand() & mask;
   }
   while (res >= range);
   return (res + a);
}*/

int randint(int a, int b){
   int range = b - a + 1;
   int res = rand() % range;
   return res + a;
}


#endif
