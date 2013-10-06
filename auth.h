#ifndef _MASS_AUTH_H
#define _MASS_AUTH_H
#include "core.h"
#include "packet.h"

typedef struct _MASS_AUTH_ARGS {
   uint8          blank;
} MASS_AUTH_ARGS;

DWORD WINAPI mass_auth_entry(void *ptr);
#endif