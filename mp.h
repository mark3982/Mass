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
   // debugging stuff
   char                       *file;
   char                       *func;
   int                        line;
} MASS_MP_PKT;

typedef struct _MASS_MP_SOCK {
   struct _MASS_MP_SOCK       *next;
   struct _MASS_MP_PKT        *prev;
   uint32                     addr;
   uint32                     bcaddr;
   MASS_MP_PKT                *in;
} MASS_MP_SOCK;

/*
   The sendtom2 is used to send a packet not only to it's destination, but also to
   send a packet to ourselves. This send to ourself reduces the complexity of the
   code by allowing code paths used remote to be used locally also.

   As you can infer sendto will NOT send to ourself and that is very useful for
   optimizing network flow for packets that are only going remotely.
*/
#define mass_net_sendtom2(mps, buf, sz, addr) _mass_net_sendto(mps, buf, sz, addr, __FILE__, __FUNCTION__, __LINE__, 1)
#define mass_net_sendto(mps, buf, sz, addr) _mass_net_sendto(mps, buf, sz, addr, __FILE__, __FUNCTION__, __LINE__, 0)

void mass_net_init();
int _mass_net_sendto(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 addr, char *file, char *func, int line, uint8 sts);
int mass_net_recvfrom(MASS_MP_SOCK *mps, void *buf, uint16 sz, uint32 *_addr);
int mass_net_create(MASS_MP_SOCK *mps, uint32 laddr, uint32 bcaddr);
int mass_net_tick(MASS_MP_SOCK *mps);

#endif