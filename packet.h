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
#define MASS_ENTITYCHECKADOPT2_TYPE             112
#define MASS_ENTITYCHECKADOPT2R_TYPE            113
#define MASS_MASTERCHILDCOUNT_TYPE              114

#define MASS_ENTITYCHECKADOPT_FINAL             0x01     // set when packet is being sent to final service

typedef struct _MASS_PACKET {
   uint32            length;
   uint16            type;
} MASS_PACKET;

typedef struct _MASS_MASTERCHILDCOUNT {
   MASS_PACKET       hdr;
   uint32            count;
} MASS_MASTERCHILDCOUNT;

// server merge check packet
typedef struct _MASS_SMCHECK {
   MASS_PACKET       hdr;
   uint32            askingID;               // ip/id for sender
   uint16            askingPort;             // port for sender
   uint16            askingDom;              // the domain id of who is asking (sender)
   f64               x, y, z;                // averaged center position for sender
   f64               irange;                 // interaction range for sender
   uint32            entityCnt;              // entity count for sender
} MASS_SMCHECK;

typedef struct _MASS_SMREPLY {
   MASS_PACKET       hdr;
   uint32            replyID;                // (sender) id of server to merge with
   uint32            replyPort;              // (sender) port of server to merge with
   uint16            replyDom;               // (sender) the dom that will merge
   uint16            checkingDom;            // the dom id of the checker (who issued the SMCHECK)
   uint32            maxCount;               // (sender) maximum number of entities that can be merged at this time
} MASS_SMREPLY;

typedef struct _MASS_ACCEPTENTITY {
   MASS_PACKET       hdr;
   uint16            dom;
   ENTITYID          eid;
} MASS_ACCEPTENTITY;

typedef struct _MASS_REJECTENTITY {
   MASS_PACKET       hdr;
   uint16            dom;
   ENTITYID          eid;
} MASS_REJECTENTITY;

typedef struct _MASS_MASTERPING {
   MASS_PACKET       hdr;
} MASS_MASTERPING;


typedef struct _MASS_ENTITYCHECKADOPT2 {
   MASS_PACKET       hdr;
   uint32            rid;                       // request id
   ENTITYID          entityID;                  // entity id
   uint32            askingServiceID;           // asking service id
   uint16            askingServiceDom;          // asking service domain
   f64               x, y, z;                   // coordinates
} MASS_ENTITYCHECKADOPT2;

typedef struct _MASS_ENTITYCHECKADOPT2R {
   MASS_PACKET       hdr;
   uint32            rid;                       // request id
   ENTITYID          entityID;                  // entity id
   uint16            askingServiceDom;          // who was specified in askingServiceDom in original packet
   uint32            domain;                    // domain for distance specified
   f64               distance;                  // distance (asking service will choose between them all)
   uint32            cpuLoad;                   // linear arbitrary value representing load
} MASS_ENTITYCHECKADOPT2R;

typedef struct _MASS_ENTITYCHECKADOPT {
   MASS_PACKET       hdr;
   ENTITYID          entityID;                  /* entity ID */
   uint32            askingServiceID;           /* asking service id/ip */
   uint16            askingServicePort;         /* asking service port */
   uint16            askingServiceDom;          /* asking service domain */
   /* best service that has other entities */
   uint32            bestServiceID;             /* best service id/ip (default zero) */
   uint16            bestServicePort;           /* best service port */
   uint16            bestServiceDom;            /* best service domain */
   f64               bestDistance;              /* best distance */
   uint64            zoneID;                    /* unused */
   /* service with lowest CPU usage */
   uint32            bestCPUID;
   uint16            bestCPUPort;
   uint16            bestCPUScore;
   f64               x, y, z;                   /* position of entity in global space */
} MASS_ENTITYCHECKADOPT;

typedef struct _MASS_ENTITYADOPT {
   MASS_PACKET       hdr;
   uint16            dom;
   uint16            fromDom;
   MASS_ENTITY       entity;
} MASS_ENTITYADOPT; 

typedef struct _MASS_ENTITYADOPTREDIRECT {
   MASS_ENTITYADOPT  hdr;
   uint32            replyID;
   uint32            replyPort;
   uint32            replyDom;
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
} MASS_SERVICEREADY;

#endif