#ifndef _MASS_CORE_H
#define _MASS_CORE_H
#include <Windows.h>

typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef float f32;
typedef double f64;
typedef unsigned long long uint64;

#define MASS_INTERACT_RANGE            100.0

;  // strange MSVC IDE error and this fixed it..

#define MASS_DISTANCE(a,b,c,x,y,z) (sqrt(a-x*a-x + b-y*b-y + c-z*c-z))

typedef struct _MASS_ENTITY {
   f64               lx, ly, lz;                   // linear position
   f64               rx, ry, rz;                   // rotational orientation
   f64               lex, ley, lez;                // linear energy
   f64               rex, rey, rez;                // rotational energy

   uint64            entityID;
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
   MASS_ENTITY                entity;
   struct _MASS_ENTITYCHAIN   *next;
} MASS_ENTITYCHAIN;

#endif