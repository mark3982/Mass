#ifndef _MASS_GHOST_H
#define _MASS_GHOST_H
#include "core.h"

typedef struct _MASS_GHOST_ARGS {
   uint32            masterAddr;
   uint16            masterPort;
   uint16            servicePort;
} MASS_GHOST_ARGS;

typedef struct _MASS_GHOSTCHILD_ARGS {
   uint32            naddr;               // next service address (or zero for end of chain)
   uint16            nport;               // next port (unused if naddr is zero)
   uint32            suraddr;             // startup reply address
   uint16            surport;             // startup reply port
   uint32            ifaceaddr;           // interface address we listen on
} MASS_GHOSTCHILD_ARGS;

DWORD WINAPI mass_ghost_entry(void *arg);

#endif