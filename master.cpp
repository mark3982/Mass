#include <time.h>
#include <stdio.h>

#include "master.h"
#include "core.h"
#include "packet.h"

#define MASS_MASTER_MAXSERVICES           128

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
   uint32                  ghostServices[MASS_MASTER_MAXSERVICES * 2];
   time_t                  ct;
   int                     f;

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

   memset(&ghostServices[0], 0, sizeof(ghostServices));

   while (1) {
      while (recvfrom(sock, (char*)buf, 0xffff, 0, (sockaddr*)&addr, &addrsz) > 0) {
         pkt = (MASS_PACKET*)buf;
         switch (pkt->type) {
            // track available game host services (we use these services to start
            // child services to actually host a domain of entities)
            case MASS_MASTERPING_TYPE:
               f = -1;
               time(&ct);
               for (int x = 0; x < MASS_MASTER_MAXSERVICES; ++x) {
                  if (ghostServices[x * 2 + 0] == addr.sin_addr.S_un.S_addr) {
                     printf("[master] updated time on ghost service %x\n", addr.sin_addr.S_un.S_addr);
                     ghostServices[x * 2 + 1] = ct;
                     break;
                  }
                  if (ghostServices[x * 2 + 0] == 0) {
                     f = x;
                  }
               }
   
               if (f > -1) {
                  printf("[master] added new ghost service %x\n", addr.sin_addr.S_un.S_addr);
                  ghostServices[f * 2 + 0] = addr.sin_addr.S_un.S_addr;
                  ghostServices[f * 2 + 1] = ct;
               }
               break;
         }
         // TODO: fix this entire mess with a select(..) call
         //       i am doing it this way at first to keep it simple
         Sleep(1000);
         // determine if we should create an entity
         if (rand() > (RAND_MAX >> 1)) {
            // do we have a running service
         }
      }
   }

   return 0;
}