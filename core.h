#ifndef _MASS_CORE_H
#define _MASS_CORE_H
#include <Windows.h>

typedef unsigned int uint32;
typedef int int32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef float f32;
typedef double f64;
typedef unsigned long long uint64;

typedef uint64 ENTITYID;

#define MASS_INTERACT_RANGE            10.0
#define MASS_MASTER_GHOST_TMEOUT       30000
#define MASS_MAXENTITY                 10
#define MASS_GHOST_GROUP               0x200
#define MASS_MASTER_ADDR               0x2001
#define MASS_MASTER_BCADDR             0x100
#define MASS_GHOST_FIRST               0x4001

#define RANDFP() ((double)rand() / (double)RAND_MAX)

;  // strange MSVC IDE error and this fixed it..

#define MASS_DISTANCE(a,b,c,x,y,z) (sqrt(((a)-(x))*((a)-(x)) + ((b)-(y))*((b)-(y)) + ((c)-(z))*((c)-(z))))

#define MASS_ENTITY_LOCKED             0x01

typedef struct _MASS_ENTITY {
   f64               lx, ly, lz;                   // linear position
   f64               rx, ry, rz;                   // rotational orientation
   f64               lex, ley, lez;                // linear energy
   f64               rex, rey, rez;                // rotational energy

   uint8             flags;                        // various flags

   ENTITYID          entityID;
   uint16            innerEntityCnt;
   uint64            modelID;                   // references the outer/inner mesh, textures, structure, modules ...ect
                                                // also references thrusters and such other things which should be stored
                                                // on the master server and may have to be downloaded

   uint64            parentEntityID;            // if we are attached to another entity
                                                // such case is if we are a human entity or entity inside another entity such as
                                                // a spaceship, station, base, ..etc.
   f64               mass;                      // the mass in kilograms of the entity
} MASS_ENTITY;

// packaged for a linked list
typedef struct _MASS_ENTITYCHAIN {
   struct _MASS_ENTITYCHAIN   *next;
   struct _MASS_ENTITYCHAIN   *prev;
   MASS_ENTITY                entity;
} MASS_ENTITYCHAIN;

#endif