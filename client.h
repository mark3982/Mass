#ifndef _MASS_CLIENT_H
#define _MASS_CLIENT_H
#include <GL\glut.h>

#include "core.h"
#include "linklist.h"

typedef struct _MASS_CLIENT_ARGS {
   int         argc;
   char        **argv;
} MASS_CLIENT_ARGS;

#define MASS_UI_TY_EVTPTR_MOVE         1  /* when the pointer moves to a new location */
#define MASS_UI_TY_EVTINPUT            2  /* can come from controller or keyboard */
#define MASS_UI_TY_EVTDRAG             3  /* drag detected event */ 

#define MASS_UI_IN_A                   1
#define MASS_UI_IN_B                   2
#define MASS_UI_IN_X                   3
#define MASS_UI_IN_Y                   4
#define MASS_UI_IN_LS                  5
#define MASS_UI_IN_RS                  6
#define MASS_UI_IN_LT                  7 /* left bottom */
#define MASS_UI_IN_LB                  8 /* left top */
#define MASS_UI_IN_RT                  9
#define MASS_UI_IN_RB                 10
#define MASS_UI_IN_SELECT             11
#define MASS_UI_IN_START              12

typedef struct _MASS_UI_EVTINPUT {
   uint8             key;              /* key mapped to keyboard or controller */
   uint8             pushed;           /* release == !pushed */
   uint32            ptrx;             /* pointer location */
   uint32            ptry;             /* pointer location */
} MASS_UI_EVTINPUT;

struct _MASS_UI_WIN;

typedef struct _MASS_UI_EVTDRAG {
   uint8                      key;     /* the key that was held */
   struct _MASS_UI_WIN        *from;   /* the window the drag started on */
   struct _MASS_UI_WIN        *to;     /* the window the drag ended on */
   uint32                     lx, ly;  /* local original click */
   int32                      dx, dy;  /* delta of x and y */
} MASS_UI_DRAG;

typedef struct _MASS_UI_EVTPTR_MOVE {
   uint32               ptrx;
   uint32               ptry;
} MASS_UI_EVTPTR_MOVE;

typedef void (*MASS_UI_CB) (_MASS_UI_WIN *win, uint32 evtype, void *ev);

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
   uint8                     *bgimgpath;       /* background image path */
   GLuint                    bgtex;            /* openGL texture identifier */
   MASS_UI_CB                cb;               /* callback function for events */
} MASS_UI_WIN;


DWORD WINAPI mass_client_entry(void *arg);
#endif