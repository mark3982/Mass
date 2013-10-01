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
   MASS_MP_PKT                *in;
} MASS_MP_SOCK;


int mass_net_sendto(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 addr, uint16 port);
int mass_net_recvfrom(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 *_addr, uint16 *_port);
int mass_net_create(MASS_MP_SOCK *mps, uint32 laddr, uint16 *lport, uint32 blm, uint32 rwt);
int mass_net_resend(MASS_MP_SOCK *mps);

#endif