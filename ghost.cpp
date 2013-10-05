#include <math.h>
#include <stdio.h>
#include <time.h>

#include "linklist.h"
#include "ghost.h"
#include "core.h"
#include "packet.h"
#include "mp.h"

/* global used to quickly find new unused id */
uint8             mapdom[0xffff / 8];

void mass_ghost_sm(MASS_MP_SOCK *sock, 
                   MASS_ENTITYCHAIN *entities, 
                   uint16 askingDom, 
                   uint32 masterID, 
                   uint32 localID,
                   uint32 rid) {
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
   smc.askingDom = askingDom;
   smc.irange = MASS_INTERACT_RANGE;
   smc.x = ax;
   smc.y = ay;
   smc.z = az;
   smc.rid = rid;

   mass_net_sendtom2(sock, &smc, sizeof(MASS_SMCHECK), MASS_GHOST_GROUP);
}

void mass_ghost_gb(MASS_MP_SOCK *sock, MASS_ENTITYCHAIN *entities, uint32 masterID, uint32 ourID, uint16 ourDom) {
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

   MASS_ENTITYCHECKADOPT2           pktca;

   pktca.hdr.type = MASS_ENTITYCHECKADOPT2_TYPE;
   pktca.hdr.length = sizeof(MASS_ENTITYCHECKADOPT2);

   pktca.askingServiceID = ourID;
   pktca.askingServiceDom = ourDom;
   pktca.rid = 0;

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
            // TODO: change to send straight to the ghost group instead of master
            mass_net_sendto(sock, &pktca, sizeof(MASS_ENTITYCHECKADOPT2), MASS_GHOST_GROUP);
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
   MASS_SERVICEREADY       sr;
   MASS_CHGSERVICENXT      *pktcsn;
   time_t                  ct;
   int                     x;
   uint32                  fromAddr;
   MASS_DOMAIN             *domains;
   MASS_MP_SOCK            sock;
   MASS_DOMAIN             *cd;
   MASS_EACREQC            *eacreqc;
   uint32                  masterChildCount;
   uint32                  lastDbgOut;

   eacreqc = 0;

   lastDbgOut = 0;

   masterChildCount = 0;

   args = (MASS_GHOSTCHILD_ARGS*)arg;

   servicePort = 0;

   domains = 0;

   mass_net_create(&sock, args->laddr, args->bcaddr);

   buf = malloc(0xffff);

   // reply that we have completed startup so we can be considered active and part of the chain
   sr.hdr.length = sizeof(MASS_SERVICEREADY);
   sr.hdr.type = MASS_SERVICEREADY_TYPE;
   printf("[child] child service started %x:%x <%x>\n", args->laddr, args->bcaddr, args->suraddr);
   // send the reply
   mass_net_sendto(&sock, &sr, sizeof(MASS_SERVICEREADY), args->suraddr);

   uint32      la = 0;
   uint32      lp = 0;

   while (1) {
         /* domain loop (drops empty domains) */
         for (cd = domains; cd ; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
            /* no entities left in this domain we need to remove it */
            if (!cd->entities) {
               /* it only removes one per cycle but that should be sufficent */
               mass_ll_rem((void**)&domains, cd);
               printf("[child:%x] dropped domain %x\n", args->laddr, cd->dom);
               free(cd);
               break;
            }
         }
         /* domain loop (tick each domain of entities) */
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
            if (ct - cd->lgb > 15) {
               cd->lgb = ct;
               printf("[child:%x:%x] doing group break..\n", args->laddr, servicePort);
               mass_ghost_gb(&sock, cd->entities, args->suraddr, args->laddr, cd->dom);
            }
            
            /* check for server merge */
            if (ct - cd->lsm > 15) {
               cd->lsm = ct;
               printf("[child:%x:%x] doing server merge check\n", args->laddr, servicePort);

               cd->sm_rid++;
               cd->sm_bestAddr = 0;
               cd->sm_bestTake = 0;
               cd->sm_bestDis = 0.0;
               cd->sm_bestDom = 0;
               cd->sm_count = masterChildCount; /* this is updated by a packet sent by the master */
               cd->sm_last = ct;

               mass_ghost_sm(&sock, cd->entities, cd->dom, args->suraddr, args->laddr, cd->sm_rid);
            }
         } /* end of domain iteration loop */
      
         /* allow network subsystem to do anything it needs to do on interval */
         mass_net_tick(&sock);

         time(&ct);

         if (ct - lastDbgOut > 10) {
            lastDbgOut = ct;
            for (cd = domains; cd; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
               for (MASS_ENTITYCHAIN *ec = cd->entities; ec; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec)) {
                  printf("[CLIENTDBG] client:%x dom:%x entity:%x\n", args->laddr, cd->dom, ec->entity.entityID);
               }
            }
         }

         /* remove old requests that were never completed, and unlock entities */
         time_t            ct;

         time(&ct);

         for (MASS_EACREQC *c = eacreqc; c != 0; c = (MASS_EACREQC*)mass_ll_next(c)) {
            if (ct - c->startTime > 30) {
               mass_ll_rem((void**)&eacreqc, c);
               // TODO: unlock entity
               printf("[child] TODO: UNLOCK ENTITIES!! LOL!!\n");
               break;
            }
         }

         while (mass_net_recvfrom(&sock, buf, 0xffff, &fromAddr)) {
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
                  f64                  bestDis;
                  uint16               bestDom;
                  uint16               bestCnt; // TODO: .. what if we needed more? (check me later)
                  uint32               bestTake;

                  pktsmc = (MASS_SMCHECK*)pkt;

                  pktsmr.hdr.length = sizeof(MASS_SMREPLY);
                  pktsmr.hdr.type = MASS_SMREPLY_TYPE;
                  pktsmr.replyID = args->laddr;
                  pktsmr.checkingDom = pktsmc->askingDom;

                  bestDom = 0;
                  bestCnt = 0;
                  bestDis = 0.0;
                  bestTake = 0;

                  /* determine if a child can adopt the entities in a server merge */
                  for (cd = domains; cd != 0; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
                     /* do not ask ourselves */
                     if (pktsmc->askingID == args->laddr && pktsmc->askingDom == cd->dom) {
                        continue;
                     } else {
                        mass_ghost_cwc(cd->entities, &x, &y, &z);
                        //x = MASS_DISTANCE(x, y, z, pktsmc->x, pktsmc->y, pktsmc->z);
                        x = x - pktsmc->x;
                        y = y - pktsmc->y;
                        z = z - pktsmc->z;
                        x = sqrt(x*x+y*y+z*z);
                     }
                     /* if we are close enough and have less than maximum entities */
                     if ((x < MASS_INTERACT_RANGE || bestDis == 0.0) && cd->ecnt < MASS_MAXENTITY) {
                        bestDom = cd->dom;
                        bestTake = MASS_MAXENTITY - cd->ecnt;
                        bestCnt = cd->ecnt;
                        bestDis = x;
                        printf("[child] SM=GOOD DOM=%x FROM:%x\n", cd->dom, pktsmc->askingID);
                     }
                  }

                  printf("[child:%x] SMCHECK DIS=%f\n", args->laddr, bestDis);

                  pktsmr.replyDom = bestDom;
                  pktsmr.maxTake = bestTake;
                  pktsmr.totalEntities = bestCnt;
                  pktsmr.distance = bestDis;

                  pktsmr.rid = pktsmc->rid;

                  mass_net_sendtom2(&sock, &pktsmr, sizeof(MASS_SMREPLY), pktsmc->askingID);
                  break;
               }
               /*
                  This happens after a SMCHECK has been sent. The first domain that is valid for the merge sends
                  this SMREPLY. This is different from the CHECKADOPT as it must propagate all domains before the
                  reply is sent back.
               */
               case MASS_SMREPLY_TYPE: 
               {
                  MASS_SMREPLY         *pktsmr;
                  MASS_ENTITYADOPT     pktea;

                  pktsmr = (MASS_SMREPLY*)pkt;

                  cd = mass_getdom(domains, pktsmr->checkingDom);

                  /* if no dom found then short-circuit out */
                  if (!cd)
                     break;

                  if (pktsmr->rid != cd->sm_rid) {
                     /* this can happen, but should be rare maybe, but if it
                        does happen I want to know and be able to tell if it
                        is happening a lot hopefully
                     */
                     printf("[child] got bad RID for SMREPLY\n");
                     break;
                  }

                  cd->sm_count--;

                  /* 
                    do not take from a bigger blob basically;
                    also trying to stop domains from constantly
                    handing back and forth and to instead build
                    domain's with large number of entities
                  */
                  if (pktsmr->totalEntities > cd->ecnt)
                     break;

                  /* these fields are initialized when the SMCHECK is sent */
                  if ((pktsmr->distance < cd->sm_bestDis || cd->sm_bestDis == 0.0) && pktsmr->distance != 0.0) {
                     cd->sm_bestAddr = fromAddr;
                     cd->sm_bestTake = pktsmr->maxTake;
                     cd->sm_bestDis = pktsmr->distance;
                     cd->sm_bestDom = pktsmr->replyDom;
                  }

                  if (cd->sm_count <= 0) {
                     /* only if we got at least one good reply */
                     if (cd->sm_bestDis == 0.0)
                        break;

                     pktea.hdr.length = sizeof(pktea);
                     pktea.hdr.type = MASS_ENTITYADOPT_TYPE;
                     pktea.fromDom = cd->dom;
                     pktea.dom = cd->sm_bestDom;

                     /* this query has completed; send some entities */
                     for (MASS_ENTITYCHAIN *ec = cd->entities; ec && cd->sm_bestTake > 0; ec = (MASS_ENTITYCHAIN*)mass_ll_next(ec)) {
                        memcpy(&pktea.entity, &ec->entity, sizeof(ec->entity));
                        mass_net_sendtom2(&sock, &pktea, sizeof(pktea), cd->sm_bestAddr);
                        cd->sm_bestTake--;
                     }
                  }
                  break;
               }
               case MASS_CHGSERVICENXT_TYPE:
                  // called to modify the chain; args is set instead of a local variable to facilitate
                  // the ability for the child service to restart after a crash and still maintain it's
                  // position in the chain since args is allocated on the heap and a reference is kept
                  // to faciliate the restart
                  pktcsn = (MASS_CHGSERVICENXT*)pkt;
                  args->naddr = pktcsn->naddr;
                  printf("[child] got CHGSERVICENXT with --%x--\n", args->naddr);
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
                     cd->sm_rid = 100;
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
                        mass_net_sendtom2(&sock, &pktre, sizeof(MASS_REJECTENTITY), fromAddr);
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
				      printf("[child] adopted entity into dom %u from dom %u\n", cd->dom, pktea->fromDom);
                  /* send accept entity so remote domain can remove entity from it's self */
                  {
                     MASS_ACCEPTENTITY       pktae;
                     pktae.eid = pktea->entity.entityID;
                     pktae.hdr.type = MASS_ACCEPTENTITY_TYPE;
                     pktae.hdr.length = sizeof(MASS_ACCEPTENTITY);
                     pktae.dom = pktea->fromDom;
                     mass_net_sendtom2(&sock, &pktae, sizeof(MASS_ACCEPTENTITY), fromAddr);
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
               case MASS_MASTERCHILDCOUNT_TYPE:
               {
                  MASS_MASTERCHILDCOUNT         *pktmcc;

                  pktmcc = (MASS_MASTERCHILDCOUNT*)pkt;

                  masterChildCount = pktmcc->count;
                  break;
               }
               case MASS_ENTITYCHECKADOPT2R_TYPE:
               {
                  MASS_ENTITYCHECKADOPT2R             *pkteca2r;
                  MASS_EACREQC                        *c;
                  MASS_ENTITYADOPT                    pktea;
                  MASS_DOMAIN                         *dom;
                  MASS_ENTITYCHAIN                    *e;
                  time_t                              ct;
                  // A. We sent out a MASS_ENTITYCHECKADOPT2 from the group break code path..

                  // We are going to get zero or more replies and we must also know
                  // when we have processed the last reply. Latency needs to be ultra
                  // low so we need a method to count or account for each reply to determine
                  // when we have them all. Once that is done or during the processing of
                  // each reply we need to determine the best g-host to send the entity adopt
                  // packet to.

                  // how to handle say for instance a g-host for some reason does not send
                  // it's packet right away and is newly started so it can not be accounted
                  // for and then five minutes later sends it.. [prolly need to have what
                  // sends the request create the structure to capture the replies so if
                  // we get a random lone message it will just be discarded]

                  //eacreqc
                  pkteca2r = (MASS_ENTITYCHECKADOPT2R*)pkt;

                  for (c = eacreqc; c != 0; c = (MASS_EACREQC*)mass_ll_next(c)) {
                     if (c->rid == pkteca2r->rid)
                        break;
                  }

                  if (!c) {
                     time(&ct);
                     c = (MASS_EACREQC*)malloc(sizeof(MASS_EACREQC));
                     memset(c, 0, sizeof(MASS_EACREQC));
                     c->rid = pkteca2r->rid;
                     c->replyCnt = masterChildCount;
                     c->startTime = ct;
                     c->bestCPUScore = ~0;
                     mass_ll_add((void**)eacreqc, c);
                  }

                  if (pkteca2r->cpuLoad < c->bestCPUScore) {
                     c->bestCPUScore = pkteca2r->cpuLoad;
                     c->bestCPUAddr = fromAddr;
                  }

                  if ((pkteca2r->distance < c->bestAdoptDistance) || c->bestAdoptDistance == 0.0) {
                     c->bestAdoptDistance = pkteca2r->distance;
                     c->bestAdoptDom = pkteca2r->domain;
                     c->bestAdoptAddr = fromAddr;
                  }

                  c->replyCnt--;
                  if (c->replyCnt <= 0) {
                     // we now have all the replies
                     mass_ll_rem((void**)&eacreqc, c);
                     
                     dom = mass_getdom(domains, pkteca2r->askingServiceDom);
                     
                     for (e = dom->entities; e != 0; e = (MASS_ENTITYCHAIN*)mass_ll_next(e)) {
                        if (e->entity.entityID == pkteca2r->entityID)
                           break;
                     }

                     if (e) {
                        pktea.hdr.length = sizeof(MASS_ENTITYADOPT);
                        pktea.hdr.type = MASS_ENTITYADOPT_TYPE;
                        memcpy(&pktea.entity, &e->entity, sizeof(MASS_ENTITY));
                        if (eacreqc->bestAdoptAddr != 0) {
                           // have dom on host that *should* accept entity (but might not)
                           pktea.fromDom = dom->dom;
                           pktea.dom = eacreqc->bestAdoptDom;
                           mass_net_sendto(&sock, &pktea, sizeof(MASS_ENTITYADOPT), eacreqc->bestAdoptAddr);
                        } else {
                           // need new dom on new host
                           pktea.fromDom = dom->dom;
                           pktea.dom = 0;             // dom not specified value
                           mass_net_sendto(&sock, &pktea, sizeof(MASS_ENTITYADOPT), eacreqc->bestCPUAddr);
                        }
                     }
                     //
                  }
                  break;
               }
               case MASS_ENTITYCHECKADOPT2_TYPE:
               {
                  MASS_ENTITYCHECKADOPT2        *pkteca2;
                  uint32                        score;
                  f64                           x, y, z;
                  uint32                        bestDom;
                  f64                           bestDis;
                  MASS_ENTITYCHECKADOPT2R       pktr;

                  pkteca2 = (MASS_ENTITYCHECKADOPT2*)pkt;

                  bestDis = 0.0;

                  for (cd = domains, score = 0; cd != 0; cd = (MASS_DOMAIN*)mass_ll_next(cd)) {
                     if (cd->ecnt > MASS_MAXENTITY)
                        continue;
                     score += cd->ecnt;
                     mass_ghost_cwc(cd->entities, &x, &y, &z);
                     x = MASS_DISTANCE(pkteca2->x, pkteca2->y, pkteca2->z, x, y, z);
                     // TODO: check if domain is already full (max entities)
                     if (x < bestDis || bestDis == 0.0) {
                        bestDom = cd->dom;
                        bestDis = x;
                     }
                  }

                  // TODO: does MASS_ENTITYCHECKADOPT2 really need a askingServiceID field??
                  pktr.hdr.length = sizeof(MASS_ENTITYCHECKADOPT2R);
                  pktr.hdr.type = MASS_ENTITYCHECKADOPT2R_TYPE;
                  pktr.domain = bestDom;
                  pktr.distance = bestDis;
                  pktr.cpuLoad = score;
                  pktr.askingServiceDom = pkteca2->askingServiceDom;
                  pktr.entityID = pkteca2->entityID;
                  pktr.rid = pkteca2->rid;

                  // send reply back to node and dom specified in request packet
                  mass_net_sendto(&sock, &pktr, sizeof(MASS_ENTITYCHECKADOPT2R), pkteca2->askingServiceID);
                  break;
               } // local scope for switch
            } // switch statement
         } // packet loop
      Sleep(10);
   } // main loop   
   
   return 0;
}

