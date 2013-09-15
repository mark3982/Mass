#ifndef _MASS_PACKET_H
#define _MASS_PACKET_H
#include "core.h"


#define MASS_ENTITYCHECKADOPT_TYPE              100
#define MASS_NEWSERVICE_TYPE                    101
#define MASS_SERVICEREADY_TYPE                  102
#define MASS_CHGSERVICENXT_TYPE                 103
#define MASS_GHOSTSHUTDOWN_TYPE                 104
#define MASS_MASTERPING_TYPE                    105
#define MASS_ENTITYADOPT_TYPE                   106
#define MASS_ACCEPTENTITY_TYPE                  107
#define MASS_REJECTENTITY_TYPE                  108
#define MASS_SMCHECK_TYPE                       109
#define MASS_SMREPLY_TYPE                       110
#define MASS_ENTITYADOPTREDIRECT_TYPE           111      // actually MASS_ENTITYADOPT_TYPE, but this allows redirect through the master

#define MASS_ENTITYCHECKADOPT_FINAL             0x01     // set when packet is being sent to final service

typedef struct _MASS_PACKET {
   uint32            length;
   uint16            type;
} MASS_PACKET;

// server merge check packet
typedef struct _MASS_SMCHECK {
   uint32            askingID;               // ip/id for sender
   uint16            askingPort;             // port for sender
   f64               x, y, z;                // averaged center position for sender
   f64               irange;                 // interaction range for sender
   uint32            entityCnt;              // entity count for sender
} MASS_SMCHECK;

typedef struct _MASS_SMREPLY {
   uint32            replyID;                // (sender) id of server to merge with
   uint32            replyPort;              // (sender) port of server to merge with
   uint32            maxCount;               // (sender) maximum number of entities that can be merged at this time
} MASS_SMREPLY;


typedef struct _MASS_ACCEPTENTITY {
   MASS_PACKET       hdr;
   ENTITYID          eid;
} MASS_ACCEPTENTITY;

typedef struct _MASS_REJECTENTITY {
   MASS_PACKET       hdr;
   ENTITYID          eid;
} MASS_REJECTENTITY;

typedef struct _MASS_MASTERPING {
   MASS_PACKET       hdr;
} MASS_MASTERPING;

typedef struct _MASS_ENTITYCHECKADOPT {
   MASS_PACKET       hdr;
   ENTITYID          entityID;
   uint32            askingServiceID;           // the IP of the service asking
   uint16            askingServicePort;         // the port for the asking service
   uint32            bestServiceID;             // starts at 0 (means not yet set)
   uint16            bestServicePort;           // 
   f64               bestDistance;              // starts at -1 (means not yet set)
   uint64            zoneID;                    // zone 0 is space and other zones are references to planet zones
   f64               x, y, z;
} MASS_ENTITYCHECKADOPT;

typedef struct _MASS_ENTITYADOPT {
   MASS_PACKET       hdr;
   MASS_ENTITY       entity;
} MASS_ENTITYADOPT; 

typedef struct _MASS_ENTITYADOPTREDIRECT {
   MASS_ENTITYADOPT  hdr;
   uint32            replyID;
   uint32            replyPort;
} MASS_ENTITYADOPTREDIRECT;

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