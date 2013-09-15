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

   ct = clock();

   addr.sin_family = AF_INET;

   for (MASS_RDP_PKT *pkt = rdp->pkts; pkt != 0; pkt = (MASS_RDP_PKT*)mass_ll_next(pkt)) {
      if (ct - pkt->tsent > rdp->rwt) {
         // resend
         addr.sin_addr.S_un.S_addr = pkt->addr;
         addr.sin_port = pkt->port;
         sendto(rdp->sock, (char*)pkt->data, pkt->sz, 0, (sockaddr*)&addr, sizeof(addr));
         pkt->tsent = ct;
      }
   }
   
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
   
   sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   
   bind(sock, (sockaddr*)&addr, sizeof(addr));

   addrsz = sizeof(addr);
   getsockname(sock, (sockaddr*)&addr, &addrsz);

   iMode = 1;
   ioctlsocket(sock, FIONBIO, &iMode);

   *lport = addr.sin_port;

   if (blm > 32)
      blm = 32;

   blm = 0xffffffff >> (32 - blm);

   rdp->sock = sock;
   rdp->bl = (uint32*)malloc(sizeof(uint32) * blm);
   rdp->bli = 0;
   rdp->blm = blm;
   rdp->rwt = rwt;
   rdp->pkts = 0;

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
         
            // if we searched the entire backlog and found no match
            if (i >= rdp->blm) {
               // read the data and adjust in buffer
               memmove(buf, ((uint8*)buf) + sizeof(MASS_RDP_HDR), rsz - sizeof(MASS_RDP_HDR));
               // put id into the backlog (double send protection)
               rdp->bl[(rdp->bli++) & rdp->blm] = uid;
               return rsz;
            }
            // ignore duplicate packet
            return 0;
         case MASS_RDP_CONTROL:
            // remove from sent packets
            uid = hdr->uid;
            for (pkt = rdp->pkts; pkt != 0; pkt = (MASS_RDP_PKT*)mass_ll_next(pkt)) {
               if (pkt->uid == uid) {
                  mass_ll_rem((void**)&rdp->pkts, pkt);
                  printf("[rdp] packet (%x) removed from outgoing log\n", uid);
                  break;
               }
            }

            if (pkt == 0) 
               printf("[rdp] could not find packet (%x) in outgoing log\n", uid);
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

   nbuf = malloc(sz + sizeof(MASS_RDP_HDR));
   hdr = (MASS_RDP_HDR*)nbuf;

   hdr->type = MASS_RDP_DATA;
   hdr->uid = uid++;

   memcpy(((uint8*)nbuf) + sizeof(MASS_RDP_HDR), buf, sz);

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
   mass_ll_add((void**)&rdp->pkts, pkt);
   
   return sendto(rdp->sock, (char*)nbuf, sz + sizeof(MASS_RDP_HDR), 0, (sockaddr*)&_addr, sizeof(_addr));
}