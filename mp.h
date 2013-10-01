// mass dummy protocol (local process communication)
#ifndef _MASS_MP_H
#define _MASS_MP_H
#include "core.h"

typedef struct _MASS_MP_PKT {
   struct _MASS_MP_PKT        *next;
   struct _MASS_MP_PKT        *prev;
   uint32                      from;
   void                       *data;
   uint32                      sz;
} MASS_MP_PKT;

typedef struct _MASS_MP_SOCK {
   struct _MASS_MP_SOCK       *next;
   struct _MASS_MP_PKT        *prev;
   uint32                     addr;
   uint32                     bcaddr;
   MASS_MP_PKT                *in;
} MASS_MP_SOCK;


void mass_net_init();
int mass_net_sendto(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 addr);
int mass_net_recvfrom(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 *_addr);
int mass_net_create(MASS_MP_SOCK *mps, uint32 laddr, uint32 bcaddr);
int mass_net_tick(MASS_MP_SOCK *mps);

#endif