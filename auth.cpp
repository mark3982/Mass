#include "auth.h"
#include "mp.h"
#include "linklist.h"
#include "stdio.h"

typedef struct _MASS_AUTH_CLIENT {
   MASS_LL_HDR                llhdr;
   SOCKET                     sock;
} MASS_AUTH_CLIENT;

DWORD WINAPI mass_auth_entry(void *ptr) {
   MASS_AUTH_ARGS          *args;
   MASS_MP_SOCK            isock;

   sockaddr_in             saddr;
   SOCKET                  nsock;
   SOCKET                  osock;
   int                     err;
   u_long                  ulval;
   int                     ival;
   MASS_AUTH_CLIENT        *clients;
   MASS_AUTH_CLIENT        *client;

   args = (MASS_AUTH_ARGS*)ptr;

   // open remote side tcp port for connections
   osock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
   saddr.sin_family = AF_INET;
   saddr.sin_port = htons(24000);
   err = bind(osock, (sockaddr*)&saddr, sizeof(saddr));

   ulval = 1;
   ioctlsocket(osock, FIONBIO, &ulval);

   listen(osock, 100);

   clients = 0;

   while (1) {
      ival = sizeof(saddr);
      nsock = accept(osock, (sockaddr*)&saddr, &ival); 
      if (nsock) {
         client = (MASS_AUTH_CLIENT*)malloc(sizeof(saddr));
         client->sock = nsock;
         mass_ll_add((void**)&clients, client);
         printf("[auth] new client\n");
      }
   }

   // loop
      // any new connections? if so accept them
         // flag connection as needing authentication first
      // any new data?
         // is sender authenticated OR trying to authenticate
            // bad:  handle authentication and/or rejection of connection
            // good: locate entity ID of account and ask on local side
            //       for server who own's that ID, alert client to process
            //       then put client on wait until we have a reply
            //          1. if reply found no entity then spawn new entity
            //          2. if reply found entity then alert client and it
            //          will take over the process from here, just route
            //          packets to the owning server of the client's entity
}