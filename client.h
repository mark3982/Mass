#ifndef _MASS_CLIENT_H
#define _MASS_CLIENT_H
#include "core.h"
#include "linklist.h"

typedef struct _MASS_CLIENT_ARGS {
   int         argc;
   char        **argv;
} MASS_CLIENT_ARGS;

typedef struct _MASS_UI_WIN {
   MASS_LL_HDR               llhdr;            /* link list header */
   uint32                    top;              /* top (y) corner */
   uint32                    left;             /* left (x) corner */
   uint32                    width;            /* window width */
   uint32                    height;           /* window height */
   f32                       r, g, b;          /* window background color */
   struct _MASS_UI_WIN       *children;        /* children windows */
   uint32                    flags;            /* flags */
   uint16                    *text;            /* window text */
   f32                       tr, tg, tb;       /* text color */
} MASS_UI_WIN;


DWORD WINAPI mass_client_entry(void *arg);
#endif