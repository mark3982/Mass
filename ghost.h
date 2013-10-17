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

/*
   This holds the result of all the replies for the request.
*/
typedef struct _MASS_EACREQC {                // Entity Adopt Check Request Container/Chain
   // TODO: make a link list header structure to contain these fields (for all structures)
   MASS_LL_HDR          llhdr;
   uint32               replyCnt;             // decremented to zero for each reply
                                              // (at zero results are evaluated)
   uint32               startTime;            // time that waiting started (used for timeout)
   uint32               bestAdoptAddr;        // default to zero (none choosen if zero)
   uint32               bestAdoptDom;         // default to zero
   f64                  bestAdoptDistance;    // default to 0.0 (not set)
   uint32               bestCPUAddr;          // default to 0 (not set)
   uint32               bestCPUScore;         // default to ~0 (not set)
   uint32               rid;
} MASS_EACREQC;

typedef struct _MASS_DOMAIN {
   MASS_LL_HDR             llhdr;
   uint16                  dom;            /* our domain id */
   MASS_ENTITYCHAIN        *entities;      /* all of our entities */
   uint32                  lgb;            /* tick timer */
   uint32                  lsm;            /* tick timer */
   uint16                  ecnt;           /* entity count for THIS domain */

   uint32                  sm_last;        /* time initiated (for timeout) */
   uint32                  sm_count;       /* replies needed for completetion */
   /* the stats for the currently choosen best */
   uint32                  sm_bestAddr;
   uint16                  sm_bestDom;
   f64                     sm_bestDis;
   uint32                  sm_bestTake;
   uint32                  sm_totalEntities;
   /* this is special and is incremented on each usage */
   uint32                  sm_rid;
} MASS_DOMAIN;

DWORD WINAPI mass_ghost_entry(void *arg);
DWORD WINAPI mass_ghost_child(void *arg);

#endif