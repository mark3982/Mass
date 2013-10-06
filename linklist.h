// workaround for bug in MSVC IDE
#define ________NOTHING
#ifndef _MASS_LINKLIST_H
#define _MASS_LINKLIST_H
#include "core.h"

typedef struct _MASS_LL_HDR {
   void           *next;
   void           *prev;
} MASS_LL_HDR;

void mass_ll_rem(void **chain, void *torem);
void mass_ll_add(void **chain, void *toadd);
void *mass_ll_next(void *ptr);
void mass_ll_addLast(void **chain, void *toadd);
void *mass_ll_pop(void **chain);
#endif