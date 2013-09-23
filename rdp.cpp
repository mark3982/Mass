// reliable datagram protocol
#include <time.h>
#include <stdio.h>

#include "linklist.h"
#include "rdp.h"

uint32                  uid = 1;

/*
   This function will iterate the sent packet linked list and resend any packets
   that have not been ackowledged in the time specified in rwt (resent wait time).
*/
int mass_rdp_resend(MASS_RDP *rdp) {
   clock_t              ct;
   sockaddr_in          addr;
   int                  x;
   MASS_RDP_PKT         *tofree;
   int                  err;

   ct = clock();

   addr.sin_family = AF_INET;

   tofree = 0;
   x = 0;
   for (MASS_RDP_PKT *pkt = rdp->pkts, *last = 0; pkt != 0; last = pkt, pkt = (MASS_RDP_PKT*)mass_ll_next(pkt)) {
      if (ct - pkt->tsent > rdp->rwt) {
         // resend
         //printf("[rdp:%x] resend of %u to %x:%x\n", GetCurrentThreadId(), pkt->uid, pkt->addr, pkt->port);

         if (tofree != 0) {
            free(tofree->data);
            free(tofree);
            tofree = 0;
         }

         addr.sin_addr.S_un.S_addr = pkt->addr;
         addr.sin_port = pkt->port;
         err = sendto(rdp->sock, (char*)pkt->data, pkt->sz, 0, (sockaddr*)&addr, sizeof(addr));
         if (err < 0) {
            printf("err:%i code:%i\n", err, GetLastError());
         }
         pkt->tsent = ct;
         pkt->rc++;
         if (pkt->rc > rdp->mrc) {
            mass_ll_rem((void**)&rdp->pkts, pkt);
            tofree = pkt;
            printf("[rdp:%x] (dropped) resend after %u retries of %u to %x:%x\n", GetCurrentThreadId(), pkt->rc, pkt->uid, pkt->addr, pkt->port);
         }
      }
      ++x;
   }

   x = sizeof(addr);
   getsockname(rdp->sock, (sockaddr*)&addr, &x);

   //printf("[rdp:%x] id:%x port:%x no-ack:%u\n", GetCurrentThreadId(), addr.sin_addr.S_un.S_addr, addr.sin_port, x);
   
   return 0;
}

/*
   This creates an RDP object and initializes the socket.
*/
int mass_rdp_create(MASS_RDP *rdp, uint32 laddr, uint16 *lport, uint32 blm, uint32 rwt) {
   SOCKET            sock;
   sockaddr_in       addr;
   int               addrsz;
   u_long            iMode;

   addr.sin_family = AF_INET;
   addr.sin_addr.S_un.S_addr = laddr;
   addr.sin_port = *lport;

   // manually force it (testing)
   rwt = 1000;
   
   sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   
   bind(sock, (sockaddr*)&addr, sizeof(addr));

   addrsz = sizeof(addr);
   getsockname(sock, (sockaddr*)&addr, &addrsz);

   iMode = 1;
   ioctlsocket(sock, FIONBIO, &iMode);

   *lport = addr.sin_port;
   
   printf("[rdp:%x] created on %x:%x\n", GetCurrentThreadId(), addr.sin_addr.S_un.S_addr, addr.sin_port);

   if (blm > 32)
      blm = 32;

   blm = 0xffffffff >> (32 - blm);

   rdp->sock = sock;
   rdp->bl = (uint32*)malloc(sizeof(uint32) * blm);
   rdp->bli = 0;
   rdp->blm = blm;
   rdp->rwt = rwt;
   rdp->pkts = 0;
   rdp->mrc = 10;

   memset(rdp->bl, 0, blm);
   return 1;
}

/*
   This will recieve data and handle processing control or data packets. This needs
   to be called once for each packet of data. Best used in a loop so that it can read
   all waiting data since it is non-blocking.
*/
int mass_rdp_recvfrom(MASS_RDP *rdp, void *buf, uint16 sz, uint32 *_addr, uint16 *_port) {
   MASS_RDP_HDR            *hdr;
   uint32                  uid;
   int                     rsz;
   int                     addrsz;
   sockaddr_in             addr;
   int                     i;
   MASS_RDP_PKT            *pkt;

   addrsz = sizeof(sockaddr_in);

   while ((rsz = recvfrom(rdp->sock, (char*)buf, sz, 0, (sockaddr*)&addr, &addrsz)) > 0) {
      *_addr = addr.sin_addr.S_un.S_addr;
      *_port = addr.sin_port;

      hdr = (MASS_RDP_HDR*)buf;
      switch (hdr->type) {
         case MASS_RDP_DATA:
            uid = hdr->uid;
            // see if id is already in backlog
            for (i = 0; i < rdp->blm; ++i) {
               if (rdp->bl[i] == uid) {
                  break;
               }
            }

            // send acknowledgement (regardless so they stop sending it)
            hdr = (MASS_RDP_HDR*)malloc(sizeof(MASS_RDP_HDR));
            hdr->type = MASS_RDP_CONTROL;
            hdr->uid = uid;
            sendto(rdp->sock, (char*)hdr, sizeof(MASS_RDP_HDR), 0, (sockaddr*)&addr, addrsz);
            free(hdr);         

            // if we searched the entire backlog and found no match
            if (i >= rdp->blm) {
               // read the data and adjust in buffer
               memmove(buf, ((uint8*)buf) + sizeof(MASS_RDP_HDR), rsz - sizeof(MASS_RDP_HDR));
               // put id into the backlog (double send protection)
               rdp->bl[(rdp->bli++) & rdp->blm] = uid;
               //printf("[rdp] (one) sending ack for %u\n", uid);
               *_addr = addr.sin_addr.S_un.S_addr;
               *_port = addr.sin_port;
               return rsz;
            }
            *_addr = 0;
            *_port = 0xcccc;
            // ignore duplicate packet
            //printf("[rdp] (dup) sending ack for %u\n", uid);
            return 0;
         case MASS_RDP_CONTROL:
            // remove from sent packets
            uid = hdr->uid;
            for (pkt = rdp->pkts; pkt != 0; pkt = (MASS_RDP_PKT*)mass_ll_next(pkt)) {
               if (pkt->uid == uid) {
                  mass_ll_rem((void**)&rdp->pkts, pkt);
                  if (pkt->rc > 1)
                     printf("[rdp] packet took %u resends to %x:%x\n", pkt->rc, pkt->addr, pkt->port);
                  free(pkt->data);
                  free(pkt);
                  break;
               }
            }

            // this happens when the sender sends another copy of the packet because
            // it did not get an ack, and when the other socket is finally read both copies
            // are found and it sends two acks and one ends up not finding a packet in
            // the outgoing log.. so this is normal occurance sometimes
            if (pkt == 0) 
               //printf("[rdp] could not find packet (%u) in outgoing log\n", uid);
            break;
      } // loop
   }
   return 0;
}

/*
   This will send the packet, but also place it into the outgoing packet list so it
   can be resent if no acklowlegment is recieved.
*/
int mass_rdp_sendto(MASS_RDP *rdp, void *buf, uint16 sz, uint32 addr, uint16 port) {
   void           *nbuf;
   MASS_RDP_HDR   *hdr;
   sockaddr_in    _addr;
   MASS_RDP_PKT   *pkt;
   clock_t        ct;
   int            err;

   nbuf = malloc(sz + sizeof(MASS_RDP_HDR));
   hdr = (MASS_RDP_HDR*)nbuf;

   // shove the payload up so we can write the header
   memcpy(((uint8*)nbuf) + sizeof(MASS_RDP_HDR), buf, sz);

   hdr->type = MASS_RDP_DATA;
   hdr->uid = uid++;

   _addr.sin_family = AF_INET;
   _addr.sin_addr.S_un.S_addr = addr;
   _addr.sin_port = port;

   ct = clock();
   pkt = (MASS_RDP_PKT*)malloc(sizeof(MASS_RDP_PKT));
   pkt->data = nbuf;
   pkt->sz = sz + sizeof(MASS_RDP_HDR);
   pkt->tsent = ct;
   pkt->uid = hdr->uid;
   pkt->addr = addr;
   pkt->port = port;
   pkt->rc = 0;
   mass_ll_add((void**)&rdp->pkts, pkt);

   err = sendto(rdp->sock, (char*)nbuf, sz + sizeof(MASS_RDP_HDR), 0, (sockaddr*)&_addr, sizeof(_addr));

   if (err < 0) {
		printf("[rdp] err:%i\n", err);
   }
   return err;
}