#ifndef _MASS_MASTER_H
#define _MASS_MASTER_H
#include "core.h"

typedef struct _MASS_MASTER_ARGS {
   uint32               ifaceaddr;
   uint16               servicePort;
} MASS_MASTER_ARGS;

DWORD WINAPI mass_master_entry(LPVOID arg);
#endif