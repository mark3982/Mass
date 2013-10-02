#ifndef _MASS_GHOST_H
#define _MASS_GHOST_H
#include "core.h"

typedef struct _MASS_GHOST_ARGS {
   uint32            masterAddr;
   uint32            iface;
} MASS_GHOST_ARGS;

typedef struct _MASS_GHOSTCHILD_ARGS {
   uint32            naddr;               // next service address (or zero for end of chain)
   uint32            suraddr;             // startup reply address
   uint32            laddr;               // local address
   uint32            bcaddr;              // broadcast address
} MASS_GHOSTCHILD_ARGS;

typedef struct _MASS_DOMAIN {
   struct _MASS_DOMAIN     *next;          /* next domain in chain (can be zero) */
   struct _MASS_DOMAIN     *prev;
   uint16                  dom;            /* our domain id */
   MASS_ENTITYCHAIN        *entities;      /* all of our entities */
   uint32                  lgb;            /* tick timer */
   uint32                  lsm;            /* tick timer */
   uint16                  ecnt;           /* entity count for THIS domain */
} MASS_DOMAIN;

DWORD WINAPI mass_ghost_entry(void *arg);
DWORD WINAPI mass_ghost_child(void *arg);

#endif