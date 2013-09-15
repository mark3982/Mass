#include "linklist.h"

void *mass_ll_next(void *ptr) {
   if (ptr == 0) {
      return 0;
   }
   return ((void**)ptr)[0];
}

void mass_ll_add(void **chain, void *toadd) {
   void        *ptr;

   ((void**)toadd)[0] = *chain;
   *chain = toadd;
}

void mass_ll_rem(void **chain, void *torem) {
   if (*chain == torem) {
      *chain = ((void**)*chain)[0];
      return;
   }

   if (*chain == 0)
      return;

   for (void *ptr = ((void**)*chain)[0], *last = *chain; ptr != 0; last = ptr, ptr = ((void**)ptr)[0]) {
      if (ptr == torem) {
         ((void**)last)[0] = ((void**)ptr)[0];
         
         return;
      }
   }
}
