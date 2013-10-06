#ifndef _MASS_CLIENT_H
#define _MASS_CLIENT_H
#include "core.h"

typedef struct _MASS_CLIENT_ARGS {
   int         argc;
   char        **argv;
} MASS_CLIENT_ARGS;

DWORD WINAPI mass_client_entry(void *arg);
#endif