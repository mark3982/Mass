#include <math.h>
#include <stdio.h>
#include <time.h>

#include "linklist.h"
#include "ghost.h"
#include "core.h"
#include "packet.h"
#include "rdp.h"

DWORD WINAPI mass_ghost_child(void *arg);

DWORD WINAPI mass_ghost_entry(void *arg) {
   // check for saved states and if found load each service and state into memory
   // ... [not implemented] ...

   // ...
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
   uint32                  fromAddr;
   uint16                  fromPort;
   MASS_RDP                sock;
   int                     so = 0;   // debugging [remove implementation]!!!

   addrsz = sizeof(addr);

   args = (MASS_GHOST_ARGS*)arg;

   mass_rdp_create(&sock, args->iface, &args->servicePort, 9, 100);
   
   buf = malloc(0xffff);

   while (1) {
     while (mass_rdp_recvfrom(&sock, buf, 0xffff, &fromAddr, &fromPort) > 0) {
         pkt = (MASS_PACKET*)buf;
         switch (pkt->type) {
            case MASS_NEWSERVICE_TYPE:
                  // start new service and send reply
                  pktns = (MASS_NEWSERVICE*)pkt;
                  cargs = (MASS_GHOSTCHILD_ARGS*)malloc(sizeof(MASS_GHOSTCHILD_ARGS));
                  // setup for next in chain
                  cargs->naddr = pktns->naddr;
                  cargs->nport = pktns->nport;
                  // get the child to listen on the same interface as we do
                  cargs->ifaceaddr = args->masterAddr;
                  // give reply address and port
                  cargs->suraddr = fromAddr;
                  cargs->surport = fromPort;
                  // create the service and let it startup
                  CreateThread(NULL, 0, mass_ghost_child, cargs, 0, NULL);
                  break;
         }
      }
      Sleep(1000);

      //printf("[ghost] pinging master service\n");
      pktmp.hdr.length = sizeof(pktmp);
      pktmp.hdr.type = MASS_MASTERPING_TYPE;

      if (so == 0) {
         // send a periodic ping to the master to let it know we are still alive
         mass_rdp_sendto(&sock, &pktmp,sizeof(pktmp), args->masterAddr, args->masterPort);      
         so = 1;
      }
  }
   
   return 0;
}

void mass_ghost_sm(MASS_RDP *sock, MASS_ENTITYCHAIN *entities, uint32 masterID, uint16 masterPort, uint32 localID, uint16 localPort) {
   // calculate averagedcenter point of entities
   f64            ax, ay, az;
   MASS_ENTITY    *ce;
   int            ecnt;

   ax = ay = az = 0.0;
   ecnt = 0;
   for (MASS_ENTITYCHAIN *cec = entities; cec != 0; cec = (MASS_ENTITYCHAIN*)mass_ll_next(cec)) {
      ce = &cec->entity;
      ++ecnt;
      ax += ce->lx;
      ay += ce->ly;
      az += ce->lz;
   }

   // produce average center of all entities
   ax = ax / ecnt;
   ay = ay / ecnt;
   az = az / ecnt;

   // 1. send packet out to figure out who (another child) is closest to us and in range to merge
   // **** range to merge is when weighted center of this domain reaches interaction range with the
   //      center of the other domain
   // --- consider calculated from opossite perspective [considered ... thoughts GOOD]
   // 2. get packet with results; determine if close child is larger then us
   //    2.a. if larger then try to send our entities to them
   //    2.b. if smaller then do nothing and eventually they should try to send them to us
   
   MASS_SMCHECK           smc;
   int                    addrsz;
   
   smc.hdr.length = sizeof(MASS_SMCHECK);
   smc.hdr.type = MASS_SMCHECK_TYPE;
   smc.askingID = localID;
   smc.askingPort = localPort;
   smc.irange = MASS_INTERACT_RANGE;
   smc.x = ax;
   smc.y = ay;
   smc.z = az;

   mass_rdp_sendto(sock, &smc, sizeof(MASS_SMCHECK), masterID, masterPort);
}

void mass_ghost_gb(MASS_RDP *sock, MASS_ENTITYCHAIN *entities, uint32 masterID, uint16 masterPort) {
   // calculate averagedcenter point of entities
   f64            ax, ay, az;
   MASS_ENTITY    *ce;
   int            ecnt;

   ax = ay = az = 0.0;
   ecnt = 0;
   for (MASS_ENTITYCHAIN *cec = entities; cec != 0; cec = (MASS_ENTITYCHAIN*)mass_ll_next(cec)) {
      ce = &cec->entity;
      ++ecnt;
      ax += ce->lx;
      ay += ce->ly;
      az += ce->lz;
   }

   // produce average center of all entities
   ax = ax / ecnt;
   ay = ay / ecnt;
   az = az / ecnt;

   MASS_ENTITYADOPT           pktea;

   pktea.hdr.type = MASS_ENTITYADOPTREDIRECT_TYPE;
   pktea.hdr.length = sizeof(MASS_ENTITYADOPT);

   f64            d;
   // see who is outside this average center
   for (MASS_ENTITYCHAIN *cec = entities; cec != 0; cec = (MASS_ENTITYCHAIN*)mass_ll_next(cec)) {
      ce = &cec->entity;

      d = MASS_DISTANCE(ce->lx, ce->ly, ce->lz, ax, ay, az);

      if (d > MASS_INTERACT_RANGE) {
         // try to get another service to adopt this entity
         // note to self: (game story/term) space drift is the phenominia that occurs in this system
         //               invisible atomic forces in deep space that stretch space and time
         memcpy(&pktea.entity, ce, sizeof(MASS_ENTITY));
         mass_rdp_sendto(sock, &pktea, sizeof(MASS_ENTITYADOPT), masterID, masterPort);
      }
   }
}

void mass_ghost_cwc(MASS_ENTITYCHAIN *ec, f64 *x, f64 *y, f64 *z) {
   int         cnt;
   f64         ax, ay, az;

   cnt = 0;
   ax = ay = az = 0.0;
   for (MASS_ENTITYCHAIN *ptr = ec; ptr != 0; ptr = (MASS_ENTITYCHAIN*)mass_ll_next(ptr)) {
      ++cnt;
      ax += ptr->entity.lx;
      ay += ptr->entity.ly;
      az += ptr->entity.lz;
   }

   *x = ax / cnt;
   *y = ay / cnt;
   *z = az / cnt;
}

DWORD WINAPI mass_ghost_child(void *arg) {
   sockaddr_in             addr;
   u_long                  iMode;
   int                     addrsz;
   int                     err;
   uint16                  servicePort;
   MASS_GHOSTCHILD_ARGS    *args;
   void                    *buf;
   MASS_PACKET             *pkt;
   MASS_ENTITYCHECKADOPT   *pktca;
   MASS_SERVICEREADY       sr;
   MASS_CHGSERVICENXT      *pktcsn;
   time_t                  ct;
   uint32                  lgb;
   uint32                  lsm;
   uint16                  entityCount;
    int                     x;
   uint32                  fromAddr;
   uint16                  fromPort;

   MASS_ENTITYCHAIN        *entities;
   MASS_RDP                sock;

   args = (MASS_GHOSTCHILD_ARGS*)arg;

   servicePort = 0;  

   mass_rdp_create(&sock, args->ifaceaddr, &servicePort, 10, 100);   

   entities = 0;
   entityCount = 0;

   addrsz = sizeof(addr);

   buf = malloc(0xffff);

   // reply that we have completed startup so we can be considered active and part of the chain
   sr.hdr.length = sizeof(MASS_SERVICEREADY);
   sr.hdr.type = MASS_SERVICEREADY_TYPE;
   printf("[child] child service started %x:%x <%x:%x>\n", args->ifaceaddr, servicePort, args->suraddr, args->surport);
   // send the reply
   mass_rdp_sendto(&sock, &sr, sizeof(MASS_SERVICEREADY), args->suraddr, args->surport);

   lgb = 0;
   lsm = 0;

   while (1) {
      mass_rdp_resend(&sock);
      // do duties needed for hosting of entities (sending updates to clients... etc)
      // 1. update entity positions
      for (MASS_ENTITYCHAIN *ec = entities; ec != 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec)) {
         // advance the entity position from linear energy
         ec->entity.lx += ec->entity.lex / ec->entity.mass;
         ec->entity.ly += ec->entity.ley / ec->entity.mass;
         ec->entity.lz += ec->entity.lez / ec->entity.mass;
         // advance entity orientation from rotational energy
         ec->entity.rx += ec->entity.rex / ec->entity.mass;
         ec->entity.ry += ec->entity.rey / ec->entity.mass;
         ec->entity.rz += ec->entity.rez / ec->entity.mass;
      }

      // set random energy levels
      for (MASS_ENTITYCHAIN *ec = entities; ec != 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec)) {
         ec->entity.lex = RANDFP() * 100.0;
         ec->entity.ley = RANDFP() * 100.0;
         ec->entity.lez = RANDFP() * 100.0;
      }

      // lgb (last group break)
      // at interval
         // 1. break entities into interaction groups
         // 2. if more than one group exists keep largest and place other entities up for adoption
         //         - the entities will be placed into existing child services or into new services
      time(&ct);

      if (ct - lgb > 5000) {
         //mass_ghost_gb(&sock, entities, args->suraddr, args->surport);
      }

      // lsm (last server merge)
      // at interval
         // 1. create sphere encompassing all our entities then extend sphere by interaction distance
         // 2. check with other childs if we overlap their own interaction area
         // 3. pick largest (not full) child and start adopting entities into it until it is full or we are empty 
      if (ct - lsm > 5000) {
         //mass_ghost_sm(&sock, entities, args->suraddr, args->surport, args->ifaceaddr, servicePort);
      }

      while (mass_rdp_recvfrom(&sock, buf, 0xffff, &fromAddr, &fromPort)) {
         pkt = (MASS_PACKET*)buf;
         switch (pkt->type) {
            case MASS_SMCHECK_TYPE:
            {
               MASS_SMCHECK         *pktsmc;
               MASS_SMREPLY         pktsmr;
               f64                  x, y, z;               

               pktsmc = (MASS_SMCHECK*)pkt;

               if (pktsmc->askingID == args->ifaceaddr && pktsmc->askingPort == servicePort) {
                  x = MASS_INTERACT_RANGE + 1; // i dunno i just felt the need for +1 ... LOL
               } else {
                  mass_ghost_cwc(entities, &x, &y, &z);

                  x = MASS_DISTANCE(x, y, z, pktsmc->x, pktsmc->y, pktsmc->z);
               }

               if (x < MASS_INTERACT_RANGE && entityCount < MASS_MAXENTITY) {
                  // allow the merge
                  pktsmr.hdr.length = sizeof(MASS_SMREPLY);
                  pktsmr.hdr.type = MASS_SMREPLY_TYPE;
                  pktsmr.replyID = args->ifaceaddr;
                  pktsmr.replyPort = servicePort;
                  pktsmr.maxCount = MASS_MAXENTITY - entityCount;
                  addr.sin_addr.S_un.S_addr = pktsmc->askingID;
                  addr.sin_port = pktsmc->askingPort;
                  mass_rdp_sendto(&sock, &pktsmr, sizeof(MASS_SMREPLY), fromAddr, fromPort);
                  //sendto(sock, (char*)&pktsmr, sizeof(MASS_SMREPLY), 0, (sockaddr*)&addr, sizeof(addr));
                  printf("[child] sent GOOD reply for server merge\n");
               } else {
                  // continue onward through the chain
                  mass_rdp_sendto(&sock, pktsmc, sizeof(MASS_SMCHECK), args->naddr, args->nport);
               }
               break;
            }
            case MASS_SMREPLY_TYPE:
               MASS_SMREPLY         *pktsmr;
               MASS_ENTITYADOPT      pktae;

               pktsmr = (MASS_SMREPLY*)pkt;
               // send entities to them and mark them as locked until we get an accept or reject
               pktae.hdr.type = MASS_ENTITYADOPT_TYPE;
               pktae.hdr.length = sizeof(MASS_ENTITYADOPT);
               
               // send up to maxCount entities which could be all or only one
               for (MASS_ENTITYCHAIN *ec = entities; ec != 0 && pktsmr->maxCount > 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec), --pktsmr->maxCount) {
                  // if it is not locked them send it and lock it
                  if ((ec->entity.flags & MASS_ENTITY_LOCKED) == 0) {
                     memcpy(&pktae.entity, &ec->entity, sizeof(MASS_ENTITY));
                     ec->entity.flags |= MASS_ENTITY_LOCKED;
                  }
                  // send it to the sender (addr already set from recvfrom call)
                  //sendto(sock, (char*)&pktae, sizeof(MASS_ENTITYADOPT), 0, (sockaddr*)&addr, sizeof(addr));
                  mass_rdp_sendto(&sock, &pktae, sizeof(MASS_ENTITYADOPT), fromAddr, fromPort);
               }
               break;
            case MASS_CHGSERVICENXT_TYPE:
               // called to modify the chain; args is set instead of a local variable to facilitate
               // the ability for the child service to restart after a crash and still maintain it's
               // position in the chain since args is allocated on the heap and a reference is kept
               // to faciliate the restart
               pktcsn = (MASS_CHGSERVICENXT*)pkt;
               args->naddr = pktcsn->naddr;
               args->nport = pktcsn->nport;
               printf("[child] got CHGSERVICENXT with %x--%u\n", args->naddr, args->nport);
               break;
            case MASS_GHOSTSHUTDOWN_TYPE:
               // just exit; the issuer should have checked that the service has no state to save
               // and if so can use another packet type to save state THEN shutdown; this is mainly
               // used to shutdown an idle service; i may make the default behavior to save state
               // in the future but for now this is just a very simple implementation
               return 0;
            case MASS_ENTITYADOPTREDIRECT_TYPE:
               MASS_ENTITYADOPTREDIRECT   *pktear;

               pktear = (MASS_ENTITYADOPTREDIRECT*)pkt;
               fromAddr = pktear->replyID;
               fromPort = pktear->replyPort;
            case MASS_ENTITYADOPT_TYPE:
               MASS_ENTITYADOPT           *pktea;
               MASS_ENTITYCHAIN           *ec;
         
               pktea = (MASS_ENTITYADOPT*)pkt;

               // quick check we can actually take this entity
               x = 0;
               for (MASS_ENTITYCHAIN *_ec = entities; _ec != 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec)) {
                  ++x;
               }

               if (x > MASS_MAXENTITY) {
                  // reject the entity
                  MASS_REJECTENTITY       pktre;
   
                  pktre.eid = pktea->entity.entityID;
                  pktre.hdr.type = MASS_REJECTENTITY_TYPE;
                  pktre.hdr.length = sizeof(MASS_REJECTENTITY);
                  
                  mass_rdp_sendto(&sock, &pktre, sizeof(MASS_REJECTENTITY), fromAddr, fromPort);
                  break;
               }

               // if this is our first entity it means we WERE the IDLE thread, so let us create one
               // to replace this thread 
               if (entities == 0) {
                  CreateThread(NULL, 0, mass_ghost_child, args, 0, NULL);
               }

               ec = (MASS_ENTITYCHAIN*)malloc(sizeof(MASS_ENTITYCHAIN));
            
               memcpy(&ec->entity,  &pktea->entity, sizeof(MASS_ENTITY));
               mass_ll_add((void**)&entities, ec);
               printf("[child] adopted entity %\n", ec->entity.entityID);
               // accept the entity
               {
                  MASS_ACCEPTENTITY       pktae;
              
                  pktae.eid = pktea->entity.entityID;
                  pktae.hdr.type = MASS_ACCEPTENTITY_TYPE;
                  pktae.hdr.length = sizeof(MASS_ACCEPTENTITY);
                  
                  mass_rdp_sendto(&sock, &pktae, sizeof(MASS_ACCEPTENTITY), fromAddr, fromPort);
                  ++entityCount;
               }
               break;
            case MASS_REJECTENTITY_TYPE:
               // unlock entity for adoption (it failed this adoption)
               break;
            case MASS_ACCEPTENTITY_TYPE:
               // remove the entity from our entities
               break;
            case MASS_ENTITYCHECKADOPT_TYPE:
               pktca = (MASS_ENTITYCHECKADOPT*)pkt;
               printf("[child] got check adopt for entity %x\n", pktca->entityID);
               {
                  // -1. do we have ANY entities and has this entity not been within interaction range
                  if (pktca->bestServiceID == 0 && entities == 0) {
                     // ok claim this entity but allow it to be claimed by a service that has other
                     // entities if possible; but if not we will be able to claim the entity
                     pktca->bestServiceID = args->ifaceaddr;
                     pktca->bestServicePort = servicePort;
                     pktca->bestDistance = MASS_INTERACT_RANGE + 1;
                     if (args->naddr == 0) {
                        printf("[child] (IDLE) sending check adopt back to asking service\n");
                        fromAddr = pktca->askingServiceID;
                        fromPort = pktca->askingServicePort;
                     } else {
                        printf("[child] (IDLE) sending check adopt to next child service\n");
                        fromAddr = args->naddr;
                        fromPort = args->nport;
                     }
                     mass_rdp_sendto(&sock, pktca, sizeof(MASS_ENTITYCHECKADOPT), fromAddr, fromPort);
                     break;
                  }
                  // 0. do we have room for another entity?
                  int            x;            
                  x = 0;
                  for (MASS_ENTITYCHAIN *ec = entities; ec != 0; ec = ec->next) {
                     ++x;
                  }
                  // a hardcoded value for now but could set a static define or even dynamic value
                  if (x > MASS_MAXENTITY) {
                     // pass the packet onward
                     addr.sin_addr.S_un.S_addr = args->naddr;
                     addr.sin_port = args->nport;
                     mass_rdp_sendto(&sock, pktca, sizeof(MASS_ENTITYCHECKADOPT), fromAddr, fromPort);
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
                           pktca->bestServicePort = servicePort;
                        }
                     }
                  }
               }
               // 3. send packet to next service in chain (or back to asking service)
               if (args->naddr == 0) {
                  printf("[child] sending check adopt back to asking service\n");
                  fromAddr = pktca->askingServiceID;
                  fromPort = pktca->askingServicePort;
               } else {
                  printf("[child] sending check adopt to next child service\n");
                  fromAddr = args->naddr;
                  fromPort = args->nport;
               }
               mass_rdp_sendto(&sock, pktca, sizeof(MASS_ENTITYCHECKADOPT), fromAddr, fromPort);
               break;
         }
      }
      Sleep(1000);
   }   
   
   return 0;
}

