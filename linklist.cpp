#include <stdio.h>

#include "linklist.h"

void *mass_ll_next(void *ptr) {
   if (ptr == 0) {
      return 0;
   }
   return ((void**)ptr)[0];
}

void *mass_ll_prev(void *ptr) {
   if (ptr == 0) {
      return 0;
   }

   return ((void**)ptr)[1];
}

void *mass_ll_last(void *chain) {
      void        *p;
      void        *l;

      for (p = chain, l = chain; p != 0; l = p, p = mass_ll_next(p)) {
      }

      return l;
}

/*
   This adds a link to the end of the chain and is the slowests add operation.
*/
void mass_ll_addLast(void **chain, void *toadd) {
   void           *p;
   void           *l;

   for (p = *chain, l = *chain; p != 0; l = p, p = mass_ll_next(p));

   /* set previous link and next link */
   ((void**)toadd)[1] = l;
   ((void**)toadd)[0] = 0;
}

/*
   This adds a link to the front of the chain and is the fastest add operation.
*/
void mass_ll_add(void **chain, void *toadd) {
   ((void**)toadd)[0] = *chain;
   ((void**)toadd)[1] = 0;       /* set previous */
   *chain = toadd;
}

void *mass_ll_pop(void **chain) {
   void           *i;

   i = *chain;

   if (i != 0)
      mass_ll_rem(chain, i);

   return i;
}

void mass_ll_rem(void **chain, void *torem) {
   /* old style
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
   } */

   void     *good;

   /* new style (check if the direction is valid and update to skip 'torem' */

   /*
      if forward pointer is valid; set next in chain prev field to our prev field
   */
   if (((void**)torem)[0]) {
      ((void**)((void**)torem)[0])[1] = ((void**)torem)[1];
      good = ((void**)torem)[0];
   }

   /*
      do the same as above except in opposite direction; normally
      I do not think we should ever have any links in the previous
      direction beyond the 'chain' variable; but if we do this tries
      to help slide the chain back down by preferring to use the
      previous link as the 'good' link as you can see below

      A<--->B<--->C<--->D
            ^
            root

      Nothing should ever be previous from the root, but I suppose
      somehow it could happen. So in this case A will be prefered
      to be the next root if we removed B instead of C being 
      choosen.

      Also anything previous to the root could be easily skipped
      when iterating the chain using the next method.
   */
   if (((void**)torem)[1]) {
      ((void**)((void**)torem)[1])[0] = ((void**)torem)[0];
      good = ((void**)torem)[1];
   }

   if (*chain == torem) {
      *chain = good;
   }

   /* just for safetly and maybe make debugging easier */
   ((void**)torem)[0] = 0;
   ((void**)torem)[1] = 0;
}
