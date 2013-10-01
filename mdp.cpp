#include "mdp.h"
#include "linklist.h"

MASS_MP_SOCK      *sockchain = 0;

int mass_net_sendto(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 addr) {
   MASS_MP_PKT          *np;

   for (MASS_MP_SOCK *cs = sockchain; cs != 0; cs = (MASS_MP_SOCK*)mass_ll_next(cs)) {
      if (addr == 0 || cs->addr == addr) {
         np = (MASS_MP_PKT*)malloc(sizeof(MASS_MP_PKT));
         np->from = mps->addr;
         np->data = buf;
         np->sz = sz;
         mass_ll_addLast((void**)&cs->in, np);
      }
   }

   return sz;
}

int mass_net_recvfrom(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 *_addr) {
   MASS_MP_PKT       *pkt;

   pkt = (MASS_MP_PKT*)mass_ll_pop((void**)&mps->in);

   if (!pkt) {
      return 0;
   }

   memcpy(buf, pkt->data, pkt->sz > sz ? sz : pkt->sz);

   return pkt->sz > sz ? sz : pkt->sz;
}

int mass_net_create(MASS_MP_SOCK *mps, uint32 laddr) {
   mps->addr = laddr;
   mps->in = 0;
   mps->next = 0;
   mps->prev = 0;

   mass_ll_add((void**)&sockchain, mps);

   return 1;
}

int mass_net_tick(MASS_MP_SOCK *mps) {
   return 1;
}