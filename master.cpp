#include <time.h>
#include <stdio.h>

#include "master.h"
#include "core.h"
#include "packet.h"
#include "linklist.h"

#define MASS_MASTER_MAXSERVICES           128

typedef struct _MASS_MASTERSERVICE {
   struct _MASS_MASTERSERVICE             *next;
   uint32                                  addr;
   uint16                                  port;
   uint32                                  lping;
} MASS_MASTERSERVICE;

typedef struct _MASS_CHILDSERVICE {
   struct _MASS_CHILDSERVICE              *next;
   uint32                                 addr;
   uint16                                 port;

   uint32                                 naddr;
   uint16                                 nport;
} MASS_CHILDSERVICE;

typedef struct _MASS_WAITINGENTITY {
   struct _MASS_WAITINGENTITY             *next;
   uint8                                  sent;
   MASS_ENTITY                            entity;
} MASS_WAITINGENTITY;

void mass_master_childStart(SOCKET sock, MASS_MASTERSERVICE *mserv) {
   MASS_NEWSERVICE         pkt;
   sockaddr_in             addr;

   pkt.hdr.type = MASS_NEWSERVICE_TYPE;
   pkt.hdr.length = sizeof(MASS_NEWSERVICE);
   // wait until it has actually started to link it into the chain of child services
   pkt.naddr = 0;
   pkt.nport = 0;   

   addr.sin_addr.S_un.S_addr = mserv->addr;
   addr.sin_port = mserv->port;
   addr.sin_family = AF_INET;
   
   sendto(sock, (char*)&pkt, sizeof(MASS_NEWSERVICE), 0, (sockaddr*)&addr, sizeof(addr));
   return;
}

DWORD WINAPI mass_master_entry(LPVOID arg) {
   SOCKET                  sock;
   sockaddr_in             addr;
   int                     addrsz;
   u_long                  iMode;
   int                     err;
   MASS_MASTER_ARGS        *args;
   void                    *buf;
   MASS_PACKET             *pkt;
   MASS_NEWSERVICE         *pktns;
   MASS_MASTERPING         pktmp;
   MASS_MASTERSERVICE      *services;
   MASS_CHILDSERVICE       *cservices;
   MASS_WAITINGENTITY      *waitingEntities;
   time_t                  ct;
   int                     f;
   printf("master up\n");

   addrsz = sizeof(addr);

   args = (MASS_MASTER_ARGS*)arg;

   sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = args->ifaceaddr;
   addr.sin_port = args->servicePort;
   
   iMode = 1;
   ioctlsocket(sock, FIONBIO, &iMode);

   err = bind(sock, (sockaddr*)&addr, sizeof(sockaddr));

   buf = malloc(0xffff);

   waitingEntities = 0;
   services = 0;
   cservices = 0;

   // make some random entities
   for (int x = 0; x < 4; ++x) {
      MASS_WAITINGENTITY      *we;
      
      we = (MASS_WAITINGENTITY*)malloc(sizeof(MASS_WAITINGENTITY));
      we->entity.entityID = x;
      we->entity.innerEntityCnt = 0;
      we->entity.lx = RANDFP() * 100.0f;
      we->entity.ly = RANDFP() * 100.0f;
      we->entity.lz = RANDFP() * 100.0f;
      we->entity.mass = 100.0f;
      we->sent = 0;

      mass_ll_add((void**)&waitingEntities, we);
   }

   while (1) {
      while (recvfrom(sock, (char*)buf, 0xffff, 0, (sockaddr*)&addr, &addrsz) > 0) {
         pkt = (MASS_PACKET*)buf;
         //printf("[master] pkt->type:%u\n", pkt->type);
         switch (pkt->type) {
            case MASS_ENTITYADOPTREDIRECT_TYPE:
               addr.sin_addr.S_un.S_addr = cservices->addr;
               addr.sin_port = cservices->port;
               sendto(sock, (char*)pkt, pkt->length, 0, (sockaddr*)&addr, sizeof(addr));               
               break;
            case MASS_SMCHECK_TYPE:
               // send to child service chain (this just does the resolution of the
               // child service chain root) other each child service would have to be
               // kept up to date with who is root of the chain.... actually if this
               // turns out bad I might just do that as it could be less overhead..
               // im worried with the volume of traffic that might have to be funneled
               // here and thus make the scale factor lower of this system
               addr.sin_addr.S_un.S_addr = cservices->addr;
               addr.sin_port = cservices->port;
               sendto(sock, (char*)pkt, pkt->length, 0, (sockaddr*)&addr, sizeof(addr));
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
            case MASS_ENTITYCHECKADOPT_TYPE:
               // at this point we have already sent the entity check adopt packet into the child chain
               // and all children have processed it and now the result has arrived and after inspecting
               // the result we will actually send the adopt packet for the best child to adopt the entity
               MASS_ENTITYCHECKADOPT      *pkteca;
               MASS_ENTITYADOPT           pktea;               
               MASS_WAITINGENTITY         *ptr;

               pkteca = (MASS_ENTITYCHECKADOPT*)pkt;
   
               for (ptr = waitingEntities; ptr != 0; ptr = (MASS_WAITINGENTITY*)mass_ll_next(ptr)) {
                  if (pkteca->entityID == ptr->entity.entityID) {
                     pktea.hdr.type = MASS_ENTITYADOPT_TYPE;
                     pktea.hdr.length = sizeof(MASS_ENTITYADOPT);
                     memcpy(&pktea.entity, &ptr->entity, sizeof(ptr->entity));
                     addr.sin_addr.S_un.S_addr = pkteca->bestServiceID;
                     addr.sin_port = pkteca->bestServicePort;
                     sendto(sock, (char*)&pktea, sizeof(pktea), 0, (sockaddr*)&addr, sizeof(addr));
                     printf("[master] send adopt entity to child\n");
                     break;
                  }
               }

               if (ptr == 0) {
                  printf("[master] check adopt for unknown entity by ID %x\n", pkteca->entityID);
               }
               break;
            case MASS_SERVICEREADY_TYPE:
               MASS_SERVICEREADY          *pktsr;
               MASS_CHILDSERVICE          *csrv;

               // check if service already exists (caused by a crash restart)
               for (csrv = cservices; csrv != 0; csrv = (MASS_CHILDSERVICE*)mass_ll_next(csrv)) {
                  if (csrv->addr == addr.sin_addr.S_un.S_addr && csrv->port == addr.sin_port) {
                     printf("[master] (ignoring) possible child crash restart %x\n", csrv->addr);
                     break;
                  }
               }

               pktsr = (MASS_SERVICEREADY*)pkt;

               // send reply configuring the child service
               MASS_CHGSERVICENXT            pktcs;
               pktcs.hdr.length = sizeof(MASS_CHGSERVICENXT);
               pktcs.hdr.type = MASS_CHGSERVICENXT_TYPE;
               if (cservices != 0) {
                  pktcs.naddr = cservices->addr;
                  pktcs.nport = cservices->port;
               } else {
                  // i do belieive the service is already configured with zeros, but i suppose
                  // it will not hurt to do this anyway
                  pktcs.naddr = 0;
                  pktcs.nport = 0;
               }
               sendto(sock, (char*)&pktcs, sizeof(pktcs), 0, (sockaddr*)&addr, sizeof(addr));

               csrv = (MASS_CHILDSERVICE*)malloc(sizeof(MASS_CHILDSERVICE));
               csrv->addr = addr.sin_addr.S_un.S_addr;
               csrv->port = addr.sin_port;
               
               mass_ll_add((void**)&cservices, csrv);               

               printf("[master] child service added and linked into chain %x\n", csrv->addr);
               break;
            // track game services (not actual child service!)
            case MASS_MASTERPING_TYPE:
               MASS_MASTERSERVICE         *msptr;
               time(&ct);

               // do we already have an entry?
               for (msptr = services; msptr != 0; msptr = (MASS_MASTERSERVICE*)mass_ll_next(msptr)) {
                  if (msptr->addr == addr.sin_addr.S_un.S_addr && msptr->port == addr.sin_port) {
                     // yes, then update last ping time
                     msptr->lping = ct;
                     //printf("[master] ping from game service %x\n", msptr->addr);
                     break;
                  }
               }

               // did we have it?
               if (msptr == 0) {
                  // no, then add it
                  msptr = (MASS_MASTERSERVICE*)malloc(sizeof(MASS_MASTERSERVICE));
                  msptr->addr = addr.sin_addr.S_un.S_addr;
                  msptr->port = addr.sin_port;
                  msptr->lping = ct;
                  mass_ll_add((void**)&services, msptr);
                  printf("[master] new game service %x\n", msptr->addr);
                  // also, start up ONE child service
                  mass_master_childStart(sock, services);
               }
               break;
         }
         // TODO: fix this entire mess with a select(..) call
         //       i am doing it this way at first to keep it simple
         Sleep(1000);
         
         // drop services that have timed out
         time(&ct);
         for (MASS_MASTERSERVICE *ptr = services; ptr != 0; ptr = (MASS_MASTERSERVICE*)mass_ll_next(ptr)) {
            if (ct - ptr->lping > MASS_MASTER_GHOST_TMEOUT) {
               printf("[master] dropped (ping timeout) service %x\n", ptr->addr);
               mass_ll_rem((void**)&services, ptr);
               // reset (expensive but simple way ...but can be optimized)
               ptr = services;
               if (ptr == 0)
                  break;
            }
         }

         // do we have entities waiting to be assigned to a game host child?
         if (cservices != 0 && waitingEntities != 0) {
            for (MASS_WAITINGENTITY *ptr = waitingEntities; ptr != 0; ptr = (MASS_WAITINGENTITY*)mass_ll_next(ptr)) {
               // start check adopt process to determine best child service to handle entity
               MASS_ENTITYCHECKADOPT      pkteca;

               // if already sent just ignore it (need a timeout value)
               if (ptr->sent > 0)
                  continue;               

               pkteca.hdr.length = sizeof(MASS_ENTITYCHECKADOPT);
               pkteca.hdr.type = MASS_ENTITYCHECKADOPT_TYPE;
               pkteca.askingServiceID = args->ifaceaddr;
               pkteca.askingServicePort = args->servicePort;
               pkteca.bestDistance = MASS_INTERACT_RANGE * 2;
               pkteca.bestServiceID = 0;
               pkteca.bestServicePort = 0;
               pkteca.entityID = ptr->entity.entityID;
               pkteca.x = ptr->entity.lx;
               pkteca.x = ptr->entity.ly;
               pkteca.x = ptr->entity.lz;

               addr.sin_addr.S_un.S_addr = cservices->addr;
               addr.sin_port = cservices->port;
               sendto(sock, (char*)&pkteca, sizeof(pkteca), 0, (sockaddr*)&addr, sizeof(addr));
               printf("[master] send entity adopt for entity %x\n", ptr->entity.entityID);

               ptr->sent = 1;
            }
         }
      }
   }

   return 0;
}