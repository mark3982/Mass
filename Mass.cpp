// Mass.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdlib.h>
#include <math.h>
#include <Windows.h>
#include <WinBase.h>
#include <stdio.h>

#include "core.h"
#include "packet.h"
#include "ghost.h"
#include "master.h"
#include "mp.h"

#define SPACECHUNKSZ       256

typedef struct _TSPACECHUNK {
   uint64         x, y, z;
   uint8          *data;
} SPACECHUNK;

void mass_geo_generate_space(SPACECHUNK *chunk, uint32 seed) {


   // generate astroids
   //srand(chunk->x);
   //srand(rand() * chunk->y);
   //srand(rand() * chunk->z);
}

/*
   [create new entity]
      - broadcast new entity to root game service (id zero)
         - root game service creates a packet which contains
            1. distance to closest entity in game service domain
            and increments id by one so next service can evaluate
            
            this continues until all game services have evaluated
            and the gservice in which the entity is closest too AND
            with-in interaction distance adopts the new entity into
            it's domain

      
            if entity is not close enough to any services then a new
            service is created to host the entity


            over time entities will move around and at interval each
            entity will be evaluated (in each gservice) to see if it
            has left the interaction distance and if so a new entity
            packet will be broadcasted and the process will start again 
*/

typedef struct _MASS_IENTITY {
   f64               lx, ly, lz;                // local position inside of another entity
} MASS_IENTITY;

/*
   when a ghost recieves this packet it will evaluate the entity to find out which entity in it's domain
   is closest to this entity and if that distance is below the interaction threshold and if so does it beat
   the cloest distance (stored in bestDistance); if it does then the new best distance is set and the best
   service ID is set and the packet is handed to the next ghost in the chain

   if this is the last ghost in the chain then it will send the packet to the asking service which could be
   another ghost or another service; the asking service will then generate an entity adopt packet which will
   actually transfer the entity to the domain of the best ghost

   flood fill starting with an entity not yet reached will determine the interaction groups of entities on
   the ghost then of the groups produced the largest will be kept and the remaining will be checked for adoption
*/


/*
   when a ghost recieves this packet it will adopt the entity into it's domain
*/

/*
         [NEW ENTITY ADOPTION PROCESS]
         1. ....create a couple of gservices
         2. broadcast entity adopt packet
         3. each service evaluates and re-broadcasts packet also incrementing the nextServiceID
         4. once packet reaches last service the packet is re-broadcast with nextServieID set to 0xffff, which, will
            then be recieved by all services and the service speciied under bestServiceID will adopt the entity into
            it's interaction domain and host the entity
         5. if no service adopts the entity then a new service will be created

         [EXISTING ENTITY PROCESS FOR EACH SERVICE]
         1. iterate through all hosted entities and determine entities which no longer are with-in interaction
            distance with ANY other entities in the domain, these are placed for adoption, and a entity check
            adopt process is undergone for each of these entities
*/

/*
   assigns entities to game servers; only one of these;

   tracks all entities in the universe and assigns them to
   servers
*/

typedef struct _MASS_ENDPOINT {
   uint32            addr;
   uint16            port;
} MASS_ENDPOINT;

typedef uint32 MASS_CC_ADDR;
typedef uint16 MASS_CC_PORT;


DWORD WINAPI mass_client_entry(void *arg) {
   return 0;
}

// packet stream packet
typedef struct _MASS_PSP {
   struct _MASS_PSP     *next;
   struct _MASS_PSP     *prev;
   void                 *data;
   uint16               sz;
} MASS_PSP;

// packet stream
typedef struct _MASS_PS {
   SOCKET            socket;
   MASS_PSP          *packets;
} MASS_PS;

int mass_ps_wrap(SOCKET socket) {
   return 0;
}

int mass_ps_send(MASS_PS *ps, void *data, uint16 sz) {
   return 0;
}

int mass_ps_recv(MASS_PS *ps) {
   return 0;
}

#define MASS_CC_AUTHED        0x0001

// client connection
typedef struct _MASS_CC {
   MASS_CC_ADDR      addr;
   MASS_CC_PORT      port;
   MASS_PS           *ps;
   uint32            flags;
} MASS_CC;

/*
   provides the bridge between the client to the game servers and geo servers through
   a single pipe
*/
DWORD WINAPI mass_auth_entry(void *arg) {

   if (1) {
      return 0;
   }

   // create socket to listen for incoming connections
   SOCKET      sock;
   SOCKET      nsock;

   sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

   sockaddr_in    addr;
   int            err;
   u_long         iMode;

   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = inet_addr("0.0.0.0");
   addr.sin_port = 24550;
   
   ioctlsocket(sock, FIONBIO, &iMode);

   err = bind(sock, (sockaddr*)&addr, sizeof(sockaddr));

   listen(sock, 100);

   // loop
   while (1) {
      // check for new connections
      nsock = accept(sock, NULL, NULL);

      // recv on existing connections
      // iterate packet stream socketsl
         // iterate packets
            // process packet
   }

   return 0;
}

/*
   geography server handles chunks; there can be many geography servers each storing
   arbitrary chunks; these also generate chunks
*/
DWORD WINAPI mass_geo_entry(void *arg) {
   return 0;
}

#include "rdp.h"

int _tmain(int argc, _TCHAR* argv[])
{
   WSADATA                 wsaData;

   MASS_MASTER_ARGS        masterargs;
   MASS_GHOSTCHILD_ARGS    childargs[2];

   WSAStartup(MAKEWORD(2, 2), &wsaData);
   mass_net_init();

   /*
   // RDP TEST
   MASS_RDP    ardp, brdp;
   uint16      aport, bport;
   uint32      addr;
   uint16      port;
   char        *o;
   char        *s = "abcdefghijklmnopqrstuvwxyz";
   int         x;
   uint32      iface;

   o = (char*)malloc(256);

   aport = 0;
   bport = 0;

   iface = inet_addr("127.0.0.1");

   mass_rdp_create(&ardp, iface, &aport, 8, 100);
   mass_rdp_create(&brdp, iface, &bport, 8, 100);

   x = mass_rdp_sendto(&ardp, s, 26, iface, bport);
   
   while (1) {
      x = mass_rdp_recvfrom(&brdp, o, 256, &addr, &port);
      x = mass_rdp_recvfrom(&ardp, o, 256, &addr, &port);
   }

   if (1)
      return 0;
   */

   //CreateThread(NULL, 0, mass_auth_entry, 0, 0, NULL);
   //CreateThread(NULL, 0, mass_geo_entry, 0, 0, NULL);
   masterargs.laddr = MASS_MASTER_ADDR;
   masterargs.bcaddr = MASS_MASTER_BCADDR;
   CreateThread(NULL, 0, mass_master_entry, &masterargs, 0, NULL);

   printf("[mass] waiting for master service to start\n");
   Sleep(1000);

   childargs[0].laddr = 0x1001;
   childargs[0].bcaddr = MASS_GHOST_GROUP;
   childargs[0].naddr = 0;
   childargs[0].suraddr = MASS_MASTER_ADDR;
   CreateThread(NULL, 0, mass_ghost_child, &childargs[0], 0, NULL);

   childargs[1].laddr = 0x1002;
   childargs[1].bcaddr = MASS_GHOST_GROUP;
   childargs[1].naddr = 0;
   childargs[1].suraddr = MASS_MASTER_ADDR;
   CreateThread(NULL, 0, mass_ghost_child, &childargs[1], 0, NULL);

   for(;;) {
      Sleep(1000);
   }

	return 0;
}

