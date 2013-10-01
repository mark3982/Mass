#include "linklist.h"

void *mass_ll_next(void *ptr) {
   if (ptr == 0) {
      return 0;
   }
   return ((void**)ptr)[0];
}

void mass_ll_addLast(void **chain, void *toadd) {
   void           *p;
   void           *l;

   for (p = *chain, l = *chain; p != 0; l = p, p = mass_ll_next(p));

   if (l == 0) {
      *chain = toadd;
   } else {
      ((void**)p)[0] = toadd;
   }

   ((void**)toadd)[0] = 0;
}

void mass_ll_add(void **chain, void *toadd) {
   ((void**)toadd)[0] = *chain;
   *chain = toadd;
}

void *mass_ll_pop(void **chain) {
   void           *i;

   i = *chain;

   if (i != 0)
      mass_ll_rem(chain, i);

   return i;
}

// potential major performance problem
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
