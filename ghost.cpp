#include <math.h>
#include "ghost.h"
#include "core.h"
#include "packet.h"

DWORD WINAPI mass_ghost_child(void *arg);

DWORD WINAPI mass_ghost_entry(void *arg) {
   // check for saved states and if found load each service and state into memory
   // ... [not implemented] ...

   // ...
   SOCKET                  sock;
   sockaddr_in             addr;
   int                     addrsz;
   u_long                  iMode;
   int                     err;
   MASS_GHOST_ARGS         *args;
   MASS_GHOSTCHILD_ARGS    *cargs;
   void                    *buf;
   MASS_PACKET             *pkt;
   MASS_NEWSERVICE         *pktns;
   MASS_MASTERPING         pktmp;

   addrsz = sizeof(addr);

   args = (MASS_GHOST_ARGS*)arg;

   sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = inet_addr("0.0.0.0");
   addr.sin_port = args->servicePort;
   
   iMode = 1;
   ioctlsocket(sock, FIONBIO, &iMode);

   err = bind(sock, (sockaddr*)&addr, sizeof(sockaddr));

   buf = malloc(0xffff);

   while (1) {
     while (recvfrom(sock, (char*)buf, 0xffff, 0, (sockaddr*)&addr, &addrsz) > 0) {
         pkt = (MASS_PACKET*)buf;
         switch (pkt->type) {
            case MASS_NEWSERVICE_TYPE:
                  // start new service and send reply
                  pktns = (MASS_NEWSERVICE*)pkt;
                  cargs = (MASS_GHOSTCHILD_ARGS*)malloc(sizeof(MASS_GHOSTCHILD_ARGS));
                  // setup for next in chain
                  cargs->naddr = pktns->naddr;
                  cargs->nport = pktns->nport;
                  // give reply address and port
                  cargs->suraddr = addr.sin_addr.S_un.S_addr;
                  cargs->surport = addr.sin_port;
                  // create the service and let it startup
                  CreateThread(NULL, 0, mass_ghost_child, cargs, 0, NULL);
                  break;
         }
      }
      Sleep(1000);
      pktmp.hdr.length = sizeof(pktmp);
      pktmp.hdr.type = MASS_MASTERPING_TYPE;

      // send a periodic ping to the master to let it know we are still alive
      addr.sin_addr.S_un.S_addr = args->masterAddr;
      addr.sin_port = args->masterPort;
      sendto(sock, (char*)&pktmp, sizeof(pktmp), 0, (sockaddr*)&addr, addrsz);      
  }
   
   return 0;
}

DWORD WINAPI mass_ghost_child(void *arg) {
   SOCKET                  sock;
   sockaddr_in             addr;
   u_long                  iMode;
   int                     addrsz;
   int                     err;
   MASS_GHOSTCHILD_ARGS    *args;
   void                    *buf;
   MASS_PACKET             *pkt;
   MASS_ENTITYCHECKADOPT   *pktca;
   MASS_SERVICEREADY       *sr;
   MASS_CHGSERVICENXT      *pktcsn;

   MASS_ENTITYCHAIN        *entities;

   entities = 0;

   args = (MASS_GHOSTCHILD_ARGS*)arg;

   sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = args->ifaceaddr;
   addr.sin_port = 0;
   
   iMode = 1;
   ioctlsocket(sock, FIONBIO, &iMode);

   err = bind(sock, (sockaddr*)&addr, sizeof(sockaddr));

   addrsz = sizeof(addr);
   getsockname(sock, (sockaddr*)&addr, &addrsz); 

   buf = malloc(0xffff);

   // reply that we have completed startup so we can be considered active and part of the chain
   sr = (MASS_SERVICEREADY*)malloc(sizeof(MASS_SERVICEREADY));
   sr->hdr.length = sizeof(MASS_SERVICEREADY);
   sr->hdr.type = MASS_SERVICEREADY_TYPE;
   sr->servicePort = addr.sin_port;                // highly important to know what port this service runs on

   // send the reply
   addr.sin_addr.S_un.S_addr = args->suraddr;
   addr.sin_port = args->surport;
   sendto(sock, (char*)&sr, sizeof(sr), 0, (sockaddr*)&addr, addrsz);

   while (1) {
      // do duties needed for hosting of entities (sending updates to clients... etc)
      // 1. update entity positions
      for (MASS_ENTITYCHAIN *ec = entities; ec != 0; ec = ec->next) {
         // advance the entity position from linear energy
         ec->entity.lx += ec->entity.lex / ec->entity.mass;
         ec->entity.ly += ec->entity.ley / ec->entity.mass;
         ec->entity.lz += ec->entity.lez / ec->entity.mass;
         // advance entity orientation from rotational energy
         ec->entity.rx += ec->entity.rex / ec->entity.mass;
         ec->entity.ry += ec->entity.rey / ec->entity.mass;
         ec->entity.rz += ec->entity.rez / ec->entity.mass;
      }

      // check for incoming messages on inner-network socket
      while (recv(sock, (char*)buf, 0xffff, 0) > 0) {
         pkt = (MASS_PACKET*)buf;
         switch (pkt->type) {
            case MASS_CHGSERVICENXT_TYPE:
         
       // called when a new service is added to the service chain and the new service
               // was not added to the front of the chain (note: adding service to front of the
               // chain could be better); and have master server as root of chain)
               pktcsn = (MASS_CHGSERVICENXT*)pkt;
               args->naddr = pktcsn->naddr;
               args->nport = pktcsn->nport;
               break;
            case MASS_GHOSTSHUTDOWN_TYPE:
               // just exit; the issuer should have checked that the service has no state to save
               // and if so can use another packet type to save state THEN shutdown; this is mainly
               // used to shutdown an idle service; i may make the default behavior to save state
               // in the future but for now this is just a very simple implementation
               return 0;
            case MASS_ENTITYCHECKADOPT_TYPE:
               pktca = (MASS_ENTITYCHECKADOPT*)pkt;
               {
                  // -1. do we have ANY entities and has this entity not been within interaction range
                  if (pktca->bestServiceID == 0 && entities == 0) {
                     // ok claim this entity but allow it to be claimed by a service that has other
                     // entities if possible; but if not we will be able to claim the entity
                     pktca->bestServiceID = args->ifaceaddr;
                     pktca->bestDistance = MASS_INTERACT_RANGE + 1;
                     addr.sin_addr.S_un.S_addr = args->naddr;
                     addr.sin_port = args->nport;
                     sendto(sock, (char*)pktca, sizeof(MASS_ENTITYCHECKADOPT), 0, (sockaddr*)&addr, addrsz);                     
                     break;
                  }
                  // 0. do we have room for another entity?
                  int            x;            
                  x = 0;
                  for (MASS_ENTITYCHAIN *ec = entities; ec != 0; ec = ec->next) {
                     ++x;
                  }
                  // a hardcoded value for now but could set a static define or even dynamic value
                  if (x > 10) {
                     addr.sin_addr.S_un.S_addr = args->naddr;
                     addr.sin_port = args->nport;
                     sendto(sock, (char*)pktca, sizeof(MASS_ENTITYCHECKADOPT), 0, (sockaddr*)&addr, addrsz);
                     break;
                  }
                  // 1. check if entity is with-in interaction range of one existing entities
                  f64            d;
                  for (MASS_ENTITYCHAIN *ec = entities; ec != 0; ec = ec->next) {
                     d = MASS_DISTANCE(ec->entity.lx, ec->entity.ly, ec->entity.lz, pktca->x, pktca->y, pktca->z);
                     // 2. check if distance to us is closer than any other specified in packet
                     if (d <= MASS_INTERACT_RANGE) {
                        if (d < pktca->bestDistance) {
                           pktca->bestDistance = d;
                           pktca->bestServiceID = args->ifaceaddr;
                        }
                     }
                  }
               }
               // 3. send packet to next service in chain (or back to asking service)
               if (args->naddr == 0) {
                  addr.sin_addr.S_un.S_addr = pktca->askingServiceID;
                  addr.sin_port = pktca->askingServicePort;
               } else {
                  addr.sin_addr.S_un.S_addr = args->naddr;
                  addr.sin_port = args->nport;
               }
               sendto(sock, (char*)pktca, sizeof(MASS_ENTITYCHECKADOPT), 0, (sockaddr*)&addr, addrsz);
               break;
         }
      }
      
   }   
   
   return 0;
}

