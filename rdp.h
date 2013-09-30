// reliable datagram protocol
#ifndef _MASS_RDP_H
#define _MASS_RDP_H
#include "core.h"

typedef struct _MASS_RDP_PKT {
   struct _MASS_RDP_PKT    *next;
   struct _MASS_RDP_PKT    *prev;
   uint32                  addr;             // address sent to
   uint16                  port;             // port sent to
   void                    *data;            // pointer to data
   uint16                  sz;               // size in bytes of data
   uint32                  tsent;            // time sent (internal)
   uint32                  uid;              // unique identifier (internal)
   uint32                  rc;               // resend count (count before just giving up)
} MASS_RDP_PKT;

typedef struct _MASS_RDP {
   SOCKET            sock;                   // socket
   uint32            *bl;                    // backlog array
   uint32            bli;                    // backlog index
   uint32            blm;                    // backlog mask (or backlog size)
   uint32            rwt;                    // resend wait time
   uint32            mrc;                    // max resend count
   MASS_RDP_PKT      *pkts;
} MASS_RDP;

typedef struct _MASS_RDP_HDR {
   uint8                type;
   uint32               uid;
} MASS_RDP_HDR;

#define MASS_RDP_DATA         0x01
#define MASS_RDP_CONTROL      0x02

int mass_rdp_sendto(MASS_RDP *rdp, void *buf, uint16 sz, uint32 addr, uint16 port);
int mass_rdp_recvfrom(MASS_RDP *rdp, void *buf, uint16 sz, uint32 *_addr, uint16 *_port);
int mass_rdp_create(MASS_RDP *rdp, uint32 laddr, uint16 *lport, uint32 blm, uint32 rwt);
int mass_rdp_resend(MASS_RDP *rdp);
#endif