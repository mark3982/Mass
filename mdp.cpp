#include "mp.h"
#include "linklist.h"
#include <time.h>

CRITICAL_SECTION  mutex;
MASS_MP_SOCK      *sockchain = 0;

uint32            byteOutTotal;
uint32            byteOutTotal10;
uint32            lastCheck;
uint32            startTime;
f64               mdp_byteOutAvg;
f64               mdp_byteOutAvg10;

void mass_net_init() {
   InitializeCriticalSection(&mutex);
   byteOutTotal = 0;
   mdp_byteOutAvg = 0.0;

   time_t               ct;

   time(&ct);

   startTime = ct;
}

int _mass_net_sendto(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 addr, char *file, char *func, int line, uint8 sts) {
   MASS_MP_PKT          *np;
   time_t               ct;

   EnterCriticalSection(&mutex);

   time(&ct);

   byteOutTotal += sz;
   byteOutTotal10 += sz;

   if (ct - lastCheck >= 10) {
      mdp_byteOutAvg = (f64)byteOutTotal / (ct - startTime);
      mdp_byteOutAvg10 = (f64)byteOutTotal10 / 10.0;
      byteOutTotal10 = 0;
      lastCheck = ct;
   }

   for (MASS_MP_SOCK *cs = sockchain; cs != 0; cs = (MASS_MP_SOCK*)mass_ll_next(cs)) {
      // do not send to ourselves
      if (mps->addr == addr && sts == 0)
         continue;
      // send to everyone if addr is zero, but if not only if addr matches
      // the local address or/and the broadcast address
      if (addr == 0 || cs->addr == addr || cs->bcaddr == addr) {
         np = (MASS_MP_PKT*)malloc(sizeof(MASS_MP_PKT));
         np->from = mps->addr;
         np->data = malloc(sz);
         memcpy(np->data, buf, sz);
         np->sz = sz;
         np->file = file;
         np->func = func;
         np->line = line;
         mass_ll_addLast((void**)&cs->in, np);
      }
   }

   LeaveCriticalSection(&mutex);
   return sz;
}

int mass_net_recvfrom(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 *_addr) {
   MASS_MP_PKT       *pkt;

   EnterCriticalSection(&mutex);

   pkt = (MASS_MP_PKT*)mass_ll_pop((void**)&mps->in);

   if (!pkt) {
      LeaveCriticalSection(&mutex);
      return 0;
   }

   memcpy(buf, pkt->data, pkt->sz > sz ? sz : pkt->sz);
   *_addr = pkt->from;

   free(pkt->data);
   free(pkt);

   LeaveCriticalSection(&mutex);
   return pkt->sz > sz ? sz : pkt->sz;
}

int mass_net_create(MASS_MP_SOCK *mps, uint32 laddr, uint32 bcaddr) {
   mps->addr = laddr;
   mps->in = 0;
   mps->next = 0;
   mps->prev = 0;
   mps->bcaddr = bcaddr;

   EnterCriticalSection(&mutex);

   mass_ll_add((void**)&sockchain, mps);

   LeaveCriticalSection(&mutex);
   return 1;
}

int mass_net_tick(MASS_MP_SOCK *mps) {
   return 1;
}