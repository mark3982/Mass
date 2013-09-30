#include <math.h>
#include <stdio.h>
#include <time.h>

#include "linklist.h"
#include "ghost.h"
#include "core.h"
#include "packet.h"
#include "rdp.h"

/* global used to quickly find new unused id */
uint8             mapdom[0xffff / 8];

void mass_ghost_sm(MASS_RDP *sock, MASS_ENTITYCHAIN *entities, uint16 askingDom, uint32 masterID, uint16 masterPort, uint32 localID, uint16 localPort) {
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
   
   smc.hdr.length = sizeof(MASS_SMCHECK);
   smc.hdr.type = MASS_SMCHECK_TYPE;
   smc.askingID = localID;
   smc.askingPort = localPort;
   smc.askingDom = askingDom;
   smc.irange = MASS_INTERACT_RANGE;
   smc.x = ax;
   smc.y = ay;
   smc.z = az;

   mass_rdp_sendto(sock, &smc, sizeof(MASS_SMCHECK), masterID, masterPort);
}

void mass_ghost_gb(MASS_RDP *sock, MASS_ENTITYCHAIN *entities, uint32 masterID, uint16 masterPort, uint32 ourID, uint16 ourPort, uint16 ourDom) {
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

   MASS_ENTITYCHECKADOPT           pktca;

   pktca.hdr.type = MASS_ENTITYADOPTREDIRECT_TYPE;
   pktca.hdr.length = sizeof(MASS_ENTITYADOPT);

   pktca.askingServiceID = 0;
   pktca.bestCPUID = 0;
   pktca.bestServiceID = 0;
   pktca.askingServiceDom = ourDom;
   pktca.askingServiceID = ourID;
   pktca.askingServicePort = ourPort;

   f64            d;
   /* check all entities outside average center */
   for (MASS_ENTITYCHAIN *cec = entities; cec != 0; cec = (MASS_ENTITYCHAIN*)mass_ll_next(cec)) {
      ce = &cec->entity;

      d = MASS_DISTANCE(ce->lx, ce->ly, ce->lz, ax, ay, az);

      if (d > MASS_INTERACT_RANGE) {
         // try to get another service to adopt this entity
         // note to self: (game story/term) space drift is the phenominia that occurs in this system
         //               invisible atomic forces in deep space that stretch space and time

         /* fill required fields and send packet */
         if ((cec->entity.flags & MASS_ENTITY_LOCKED) == 0) {
            pktca.entityID = cec->entity.entityID;
            pktca.x = cec->entity.lx;
            pktca.y = cec->entity.ly;
            pktca.z = cec->entity.lz;
            cec->entity.flags |= MASS_ENTITY_LOCKED;
            mass_rdp_sendto(sock, &pktca, sizeof(MASS_ENTITYADOPT), masterID, masterPort);
         }
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

MASS_DOMAIN *mass_getdom(MASS_DOMAIN *doms, uint16 id) {
   for (MASS_DOMAIN *cd = doms; cd != 0; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
      if (cd->dom == id)
         return cd;
   }
   return 0;
}

void mass_freedomid(uint16 dom) {
   uint16      byp, bip;
   
   byp = dom / 8;
   bip = dom - (byp * 8);

   mapdom[byp] = mapdom[byp] & ~(1 << bip);
}

/* The domain ID zero is reserved. */
int32 mass_getnewdomid(MASS_DOMAIN *doms) {
   uint16         byp, bip;

   for (int x = 1; x < 0xffff / 8; ++x) {
      byp = x / 8;
      bip = x - (byp * 8);
      if (((mapdom[byp] >> bip) & 1) == 0) {
         mapdom[byp] = mapdom[byp] | (1 << bip);
         return x;
      }
   }
   return -1; 
}

DWORD WINAPI mass_ghost_child(void *arg) {
   uint16                  servicePort;
   MASS_GHOSTCHILD_ARGS    *args;
   void                    *buf;
   MASS_PACKET             *pkt;
   MASS_ENTITYCHECKADOPT   *pktca;
   MASS_SERVICEREADY       sr;
   MASS_CHGSERVICENXT      *pktcsn;
   time_t                  ct;
   int                     x;
   uint32                  fromAddr;
   uint16                  fromPort;
   int                     x_firstadopt; // bad hackery
   MASS_DOMAIN             *domains;
   MASS_RDP                sock;
   MASS_DOMAIN             *cd;

   args = (MASS_GHOSTCHILD_ARGS*)arg;

   x_firstadopt = 0;

   servicePort = 0;

   domains = 0;

   mass_rdp_create(&sock, args->ifaceaddr, &servicePort, 10, 100);   

   buf = malloc(0xffff);

   // reply that we have completed startup so we can be considered active and part of the chain
   sr.hdr.length = sizeof(MASS_SERVICEREADY);
   sr.hdr.type = MASS_SERVICEREADY_TYPE;
   printf("[child] child service started %x:%x <%x:%x>\n", args->ifaceaddr, servicePort, args->suraddr, args->surport);
   // send the reply
   mass_rdp_sendto(&sock, &sr, sizeof(MASS_SERVICEREADY), args->suraddr, args->surport);

   uint32      la = 0;
   uint32      lp = 0;

   // main loop
   while (1) {
         // domain loop (tick each domain of entities)
         for (cd = domains; cd != 0; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
            // do duties needed for hosting of entities (sending updates to clients... etc)
            // 1. update entity positions
            for (MASS_ENTITYCHAIN *ec = cd->entities; ec != 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec)) {
               // advance the entity position from linear energy
               ec->entity.lx += ec->entity.lex / ec->entity.mass;
               ec->entity.ly += ec->entity.ley / ec->entity.mass;
               ec->entity.lz += ec->entity.lez / ec->entity.mass;
               // advance entity orientation from rotational energy
               ec->entity.rx += ec->entity.rex / ec->entity.mass;
               ec->entity.ry += ec->entity.rey / ec->entity.mass;
               ec->entity.rz += ec->entity.rez / ec->entity.mass;
            }

            /* set random energy levels for testing */
            for (MASS_ENTITYCHAIN *ec = cd->entities; ec != 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec)) {
               ec->entity.lex = RANDFP() * 100.0;
               ec->entity.ley = RANDFP() * 100.0;
               ec->entity.lez = RANDFP() * 100.0;
            }

            time(&ct);

            /* check for entities outside domain interaction range limit */
            if (ct - cd->lgb > 5000) {
               cd->lgb = ct;
               printf("[child:%x:%x] doing group break..\n", args->ifaceaddr, servicePort);
               mass_ghost_gb(&sock, cd->entities, args->suraddr, args->surport, args->ifaceaddr, servicePort, cd->dom);
            }
            
            /* check for server merge */
            if (ct - cd->lsm > 5000) {
               cd->lsm = ct;
               printf("[child:%x:%x] doing server merge check\n", args->ifaceaddr, servicePort);
               mass_ghost_sm(&sock, cd->entities, cd->dom, args->suraddr, args->surport, args->ifaceaddr, servicePort);
            }
         } /* domain loop */
      
         /* resend packets with not ack received */
         mass_rdp_resend(&sock);

         //args->naddr, args->nport

         while (mass_rdp_recvfrom(&sock, buf, 0xffff, &fromAddr, &fromPort)) {
            
            if (args->naddr != la) {
               printf("CHANGED\n");
               la = args->naddr;
            }

            pkt = (MASS_PACKET*)buf;
            switch (pkt->type) {
               /*
                  The SMCHECK is when a domain needs to determine if it can merge with
                  any other domains. This has to be done on interval or otherwise entire
                  domains would move past one another and never merge. Imagine two armies
                  passing each other and seeing nothing.
               */
               case MASS_SMCHECK_TYPE:
               {
                  MASS_SMCHECK         *pktsmc;
                  MASS_SMREPLY         pktsmr;
                  f64                  x, y, z;               

                  pktsmc = (MASS_SMCHECK*)pkt;
                  printf("[child:%x:%x] SMCHECK\n", args->ifaceaddr, servicePort);
                  /* determine if a child can adopt the entities in a server merge */
                  for (cd = domains; cd != 0; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
                     /* do not ask ourselves */
                     if (pktsmc->askingID == args->ifaceaddr && pktsmc->askingPort == servicePort && pktsmc->askingDom == cd->dom) {
                        x = MASS_INTERACT_RANGE + 1;              /* just to make sure we fail the next condition */
                     } else {
                        mass_ghost_cwc(cd->entities, &x, &y, &z);
                        //x = MASS_DISTANCE(x, y, z, pktsmc->x, pktsmc->y, pktsmc->z);
                        x = x - pktsmc->x;
                        y = y - pktsmc->y;
                        z = z - pktsmc->z;
                        x = sqrt(x*x+y*y+z*z);
                     }
                     /* if we are close enough and have less than maximum entities */
                     if (x < MASS_INTERACT_RANGE && cd->ecnt < MASS_MAXENTITY) {
                        pktsmr.hdr.length = sizeof(MASS_SMREPLY);
                        pktsmr.hdr.type = MASS_SMREPLY_TYPE;
                        pktsmr.replyID = args->ifaceaddr;
                        pktsmr.replyPort = servicePort;
                        pktsmr.replyDom = cd->dom;
                        pktsmr.checkingDom = pktsmc->askingDom;
                        pktsmr.maxCount = MASS_MAXENTITY - cd->ecnt;
                        mass_rdp_sendto(&sock, &pktsmr, sizeof(MASS_SMREPLY), pktsmc->askingID, pktsmc->askingPort);
                        printf("[child] sent GOOD reply for server merge\n");
                        break;
                     }
                  }

                  /* if no match was found send to next child in chain */
                  if (cd == 0) {
                     if (args->naddr != 0)
                           mass_rdp_sendto(&sock, pktsmc, sizeof(MASS_SMCHECK), args->naddr, args->nport);
                  }
                  break;
               }
               /*
                  This happens after a SMCHECK has been sent. The first domain that is valid for the merge sends
                  this SMREPLY. This is different from the CHECKADOPT as it must propagate all domains before the
                  reply is sent back.
               */
               case MASS_SMREPLY_TYPE:
                  MASS_SMREPLY         *pktsmr;
                  MASS_ENTITYADOPT      pktae;
                  

                  pktsmr = (MASS_SMREPLY*)pkt;
                  pktae.hdr.type = MASS_ENTITYADOPT_TYPE;
                  pktae.hdr.length = sizeof(MASS_ENTITYADOPT);

                  /* get the relevant domain (on our side) */
                  cd = mass_getdom(domains, pktsmr->checkingDom);

                  /* send adopts to the domain that replied */
                  pktae.dom = pktsmr->replyDom;
                  /* what dom is sending this adopt entity packet */
                  pktae.fromDom = cd->dom;

                  /* send them as many entities as we can */
                  for (MASS_ENTITYCHAIN *ec = cd->entities; ec != 0 && pktsmr->maxCount > 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec), --pktsmr->maxCount) {
                     /* if not locked them send it AND lock it */
                     if ((ec->entity.flags & MASS_ENTITY_LOCKED) == 0) {
                        memcpy(&pktae.entity, &ec->entity, sizeof(MASS_ENTITY));
                        ec->entity.flags |= MASS_ENTITY_LOCKED;
                     }
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
               case MASS_ENTITYADOPT_TYPE:
                  MASS_ENTITYADOPT           *pktea;
                  MASS_ENTITYCHAIN           *ec;
         
                  pktea = (MASS_ENTITYADOPT*)pkt;

                  printf("[child] MASS_ENTITYADOPT packet\n");

                  /* if DOM not specified then we create a new domain and add entity */
                  if (pktea->dom == 0) {
                     printf("[child] DOM not specified; creating new\n");
                     cd = (MASS_DOMAIN*)malloc(sizeof(MASS_DOMAIN));
                     cd->dom = mass_getnewdomid(domains);
                     cd->ecnt = 0;
                     cd->entities = 0;
                     cd->lgb = 0;
                     cd->lsm = 0;
                     mass_ll_add((void**)&domains, cd);
                  } else {
                     printf("[child] DOM specified; using existing\n");
                     cd = mass_getdom(domains, pktea->dom);
                     /* make sure we are going to be under our max entity limit */
                     x = 0;
                     for (MASS_ENTITYCHAIN *_ec = cd->entities; _ec != 0; _ec = (MASS_ENTITYCHAIN*)mass_ll_next(_ec)) {
                        ++x;
                     }

                     /* reject the entity on the basis of we have too many */
                     if (x > MASS_MAXENTITY) {
                        // reject the entity
                        MASS_REJECTENTITY       pktre;
                        pktre.eid = pktea->entity.entityID;
                        pktre.hdr.type = MASS_REJECTENTITY_TYPE;
                        pktre.hdr.length = sizeof(MASS_REJECTENTITY);
                        pktre.dom = pktea->fromDom;                  
                        mass_rdp_sendto(&sock, &pktre, sizeof(MASS_REJECTENTITY), fromAddr, fromPort);
                        break;
                     }
                  }

                  /* allocate space and insert entity into the domain */
                  //malloc(sizeof(MASS_ENTITYCHAIN));
                  ec = (MASS_ENTITYCHAIN*)malloc(sizeof(MASS_ENTITYCHAIN));
                  //malloc(sizeof(MASS_ENTITYCHAIN));
                  printf("ec:%x\n", ec);
                  memcpy(&ec->entity,  &pktea->entity, sizeof(MASS_ENTITY));
                  mass_ll_add((void**)&cd->entities, ec);
				  //printf("[child] adopted entity %x into dom %u from dom %u\n", ec->entity.entityID, cd->dom, pktea->fromDom);
				  printf("[child] adopted entity into dom %u from dom %u\n", cd->dom, pktea->fromDom);
                  /* send accept entity so remote domain can remove entity from it's self */
                  {
                     MASS_ACCEPTENTITY       pktae;
                     pktae.eid = pktea->entity.entityID;
                     pktae.hdr.type = MASS_ACCEPTENTITY_TYPE;
                     pktae.hdr.length = sizeof(MASS_ACCEPTENTITY);
                     pktae.dom = pktea->fromDom;
                     mass_rdp_sendto(&sock, &pktae, sizeof(MASS_ACCEPTENTITY), fromAddr, fromPort);
                     cd->ecnt++;
                  }
                  break;
               case MASS_REJECTENTITY_TYPE:
                  /* unlock entity for adoption (it failed this adoption) */
                  MASS_REJECTENTITY       *pktre;

                  pktre = (MASS_REJECTENTITY*)pkt;

                  cd = mass_getdom(domains, pktre->dom);
                           
                  for (MASS_ENTITYCHAIN *ce = cd->entities; ce != 0; ce = (MASS_ENTITYCHAIN*)mass_ll_next(ce)) {
                     if (ce->entity.entityID == pktre->eid) {
                        /* clear the locked flag */
                        ce->entity.flags &= ~MASS_ENTITY_LOCKED;
                     }
                  }
                  break;
               case MASS_ACCEPTENTITY_TYPE:
                  MASS_ACCEPTENTITY       *_pktae;

                  _pktae = (MASS_ACCEPTENTITY*)pkt;

                  printf("_pktae->dom:%x\n", _pktae->dom);
                  cd = mass_getdom(domains, _pktae->dom);
      
                  /* remove the entity from our entity chain */
                  for (MASS_ENTITYCHAIN *ce = cd->entities; ce != 0; ce = (MASS_ENTITYCHAIN*)mass_ll_next(ce)) {
                     if (ce->entity.entityID == _pktae->eid) {
                        mass_ll_rem((void**)&cd->entities, ce);
                        printf("[child] entity %x removed from dom %x\n", _pktae->eid, _pktae->dom);
                        free(ce);
                        break;
                     }
                  }
                  break;
               case MASS_ENTITYADOPTREDIRECT_TYPE:
                  MASS_ENTITYADOPTREDIRECT   *pktear;
                  pktear = (MASS_ENTITYADOPTREDIRECT*)pkt;
                  fromAddr = pktear->replyID;
                  fromPort = pktear->replyPort;
               case MASS_ENTITYCHECKADOPT_TYPE:
                  f64         x, y, z;
                  uint32      score;

                  pktca = (MASS_ENTITYCHECKADOPT*)pkt;
                  printf("[child] got check adopt for entity %x\n", pktca->entityID);
                  /* 1. check if in interaction range of an existing domain */
                  for (cd = domains, score = 0; cd != 0; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
                     /* A. are we above our max entity count */
                     if (cd->ecnt >= MASS_MAXENTITY)
                        continue;
                     /* calculate primitive CPU usage */
                     score += cd->ecnt * cd->ecnt;
                     /* B. check if entity is with-in interaction range */
                     mass_ghost_cwc(cd->entities, &x, &y, &z);
                     x = MASS_DISTANCE(pktca->x, pktca->y, pktca->z, x, y, z);
                     if (x < MASS_INTERACT_RANGE) {
                        /* B.1. see if we can beat existing */
                        if (pktca->bestServiceID == 0 || (pktca->bestDistance > x)) {
                           pktca->bestServiceID = args->ifaceaddr;
                           pktca->askingServicePort = servicePort;
                           pktca->bestServiceDom = cd->dom;
                        }
                     }
                  }

                  if (pktca->bestCPUID == 0 || pktca->bestCPUScore > score) {
                     pktca->bestCPUID = args->ifaceaddr;
                     pktca->bestCPUPort = servicePort;
                     pktca->bestCPUScore = score;
                  }

                  if (args->naddr == 0) {
                     fromAddr = pktca->askingServiceID;
                     fromPort = pktca->askingServicePort;
                  } else {
                     fromAddr = args->naddr;
                     fromPort = args->nport;
                  }

                  mass_rdp_sendto(&sock, pktca, sizeof(MASS_ENTITYCHECKADOPT), fromAddr, fromPort);
                  break;
            }
         } // packet loop
      Sleep(10);
   } // main loop   
   
   return 0;
}

