#include <time.h>
#include <stdio.h>

#include "master.h"
#include "core.h"
#include "packet.h"
#include "linklist.h"
#include "mp.h"

#define MASS_MASTER_MAXSERVICES           128

typedef struct _MASS_MASTERSERVICE {
   struct _MASS_MASTERSERVICE             *next;
   struct _MASS_MASTERSERVICE             *prev;
   uint32                                  addr;
   uint32                                  lping;
} MASS_MASTERSERVICE;

typedef struct _MASS_CHILDSERVICE {
   struct _MASS_CHILDSERVICE              *next;
   struct _MASS_CHILDSERVICE              *prev;
   uint32                                 addr;

   uint32                                 naddr;
} MASS_CHILDSERVICE;

typedef struct _MASS_WAITINGENTITY {
   struct _MASS_WAITINGENTITY             *next;
   struct _MASS_WAITINGENTITY             *prev;
   uint8                                  sent;
   MASS_ENTITY                            entity;
} MASS_WAITINGENTITY;

DWORD WINAPI mass_master_entry(LPVOID arg) {
   MASS_MASTER_ARGS        *args;
   void                    *buf;
   MASS_PACKET             *pkt;
   MASS_MASTERSERVICE      *services;
   MASS_CHILDSERVICE       *cservices;
   MASS_WAITINGENTITY      *waitingEntities;
   time_t                  ct;
   MASS_MP_SOCK            sock;
   uint32                  fromAddr;
   MASS_MASTERCHILDCOUNT   pktmcc;

   printf("master up\n");

   args = (MASS_MASTER_ARGS*)arg;

   mass_net_create(&sock, args->laddr, args->bcaddr);

   buf = malloc(0xffff);

   waitingEntities = 0;
   services = 0;
   cservices = 0;
               
   // master child count packet prep
   pktmcc.hdr.length = sizeof(MASS_MASTERCHILDCOUNT);
   pktmcc.hdr.type = MASS_MASTERCHILDCOUNT_TYPE;
   pktmcc.count = 0;

   // make some random entities
   for (int x = 0; x < 100; ++x) {
      MASS_WAITINGENTITY      *we;
      
      we = (MASS_WAITINGENTITY*)malloc(sizeof(MASS_WAITINGENTITY));
      memset(&we->entity, 0, sizeof(MASS_ENTITY));
      we->entity.entityID = x;
      we->entity.innerEntityCnt = 0;
      we->entity.lx = 1.0; //RANDFP() * 100.0f;
      we->entity.ly = 2.0; //RANDFP() * 100.0f;
      we->entity.lz = 3.0; //RANDFP() * 100.0f;
      we->entity.mass = 1.0f;
      we->sent = 0;

      mass_ll_add((void**)&waitingEntities, we);
   }

   printf("[master] waiting for g-hosts\n");
   Sleep(1000);

   while (1) {
      Sleep(1);

      // send adoption into new domain packets to all alive g-hosts
      for (MASS_WAITINGENTITY *e = waitingEntities; e != 0; e = (MASS_WAITINGENTITY*)mass_ll_next(e)) {
         if (e->sent != 0)
            continue;
         e->sent = 1;

         MASS_ENTITYADOPT           pktea;

         pktea.hdr.length = sizeof(pktea);
         pktea.hdr.type = MASS_ENTITYADOPT_TYPE;

         pktea.fromDom = 0;
         pktea.dom = 0;

         memcpy(&pktea.entity, &e->entity, sizeof(MASS_ENTITY));
         mass_net_sendto(&sock, &pktea, sizeof(pktea), MASS_GHOST_FIRST);
         printf("[master] send adopt for %u\n", e->entity.entityID);
      }

      mass_net_tick(&sock);

      while (mass_net_recvfrom(&sock, buf, 0xffff, &fromAddr)> 0) {
         pkt = (MASS_PACKET*)buf;
         //printf("[master] pkt->type:%u\n", pkt->type);
         switch (pkt->type) {
            default:
               printf("unknown:%x\n", pkt->type);
               break;
            case MASS_ACCEPTENTITY_TYPE:
            {
               MASS_ACCEPTENTITY                *pktae;
               MASS_WAITINGENTITY               *ptr;

               pktae = (MASS_ACCEPTENTITY*)pkt;

               for (ptr = waitingEntities; ptr != 0; ptr = (MASS_WAITINGENTITY*)mass_ll_next(ptr)) {
                  if (pktae->eid == ptr->entity.entityID) {
                     // remove entity from waiting list
                     printf("[master] removed entity %x from waiting list\n", ptr->entity.entityID);
                     mass_ll_rem((void**)&waitingEntities, ptr);
                     break;
                  }
               }
               break;
            }
            case MASS_REJECTENTITY_TYPE:
            {
               MASS_REJECTENTITY                *pktre;
               MASS_WAITINGENTITY               *ptr;

               pktre = (MASS_REJECTENTITY*)pkt;

               for (ptr = waitingEntities; ptr != 0; ptr = (MASS_WAITINGENTITY*)mass_ll_next(ptr)) {
                  if (pktre->eid == ptr->entity.entityID) {
                     // allow it to be sent again
                     ptr->sent = 0;
                  }
               }               
               break;
            }
            case MASS_SERVICEREADY_TYPE:
               MASS_SERVICEREADY          *pktsr;
               MASS_CHILDSERVICE          *csrv;
               printf("[master] got MASS_SERVICEREADY_TYPE\n");

               // check if service already exists (caused by a crash restart)
               for (csrv = cservices; csrv != 0; csrv = (MASS_CHILDSERVICE*)mass_ll_next(csrv)) {
                  if (csrv->addr == fromAddr) {
                     printf("[master] (ignoring) possible child crash restart %x\n", csrv->addr);
                     break;
                  }
               }

               pktsr = (MASS_SERVICEREADY*)pkt;

               csrv = (MASS_CHILDSERVICE*)malloc(sizeof(MASS_CHILDSERVICE));
               csrv->addr = fromAddr;
               
               mass_ll_add((void**)&cservices, csrv);               

               printf("[master] child service added and linked into chain %x\n", csrv->addr);

               // keep track of the number of g-host services and relay this
               pktmcc.count++;
               mass_net_sendto(&sock, &pktmcc, sizeof(MASS_MASTERCHILDCOUNT), MASS_GHOST_GROUP);
               break;
         }
         // TODO: fix this entire mess with a select(..) call
      }
   }

   return 0;
}