#ifndef _MASS_PACKET_H
#define _MASS_PACKET_H

#include "core.h"

#define MASS_ENTITYCHECKADOPT_TYPE              100
#define MASS_NEWSERVICE_TYPE                    101
#define MASS_SERVICEREADY_TYPE                  102
#define MASS_CHGSERVICENXT_TYPE                 103
#define MASS_GHOSTSHUTDOWN_TYPE                 104
#define MASS_MASTERPING_TYPE                    105

#define MASS_ENTITYCHECKADOPT_FINAL             0x01     // set when packet is being sent to final service

typedef struct _MASS_PACKET {
   uint32            length;
   uint16            type;
} MASS_PACKET;

typedef struct _MASS_MASTERPING {
   MASS_PACKET       hdr;
} MASS_MASTERPING;

typedef struct _MASS_ENTITYCHECKADOPT {
   MASS_PACKET       hdr;
   uint64            entityID;
   uint32            askingServiceID;           // the IP of the service asking
   uint16            askingServicePort;         // the port for the asking service
   uint32            bestServiceID;             // starts at 0 (means not yet set)
   f64               bestDistance;              // starts at -1 (means not yet set)
   uint64            zoneID;                    // zone 0 is space and other zones are references to planet zones
   f64               x, y, z;
} MASS_ENTITYCHECKADOPT;

typedef struct _MASS_ENTITYADOPT {
   MASS_PACKET       hdr;
   //MASS_ENTITY       entity;
} MASS_ENTITYADOPT; 

typedef struct _MASS_CHGSERVICENXT {
   MASS_PACKET             hdr;
   uint32                  naddr;
   uint16                  nport;
} MASS_CHGSERVICENXT;

typedef struct _MASS_NEWSERVICE {
   MASS_PACKET             hdr;
   uint32                  naddr;               // address of next service in chain
   uint16                  nport;               // port of next service in chain
} MASS_NEWSERVICE;

typedef struct _MASS_GHOSTSHUTDOWN {
   MASS_PACKET             hdr;
} MASS_GHOSTSHUTDOWN;

typedef struct _MASS_SERVICEREADY {
   MASS_PACKET             hdr;
   uint16                  servicePort;
} MASS_SERVICEREADY;

#endif