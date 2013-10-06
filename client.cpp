#include <stdio.h>

#include "linklist.h"
#include "client.h"
#include "GL\glut.h"

typedef struct _MASS_UI_WIN {
   MASS_LL_HDR               llhdr;            /* link list header */
   uint32                    top;              /* top (y) corner */
   uint32                    left;             /* left (x) corner */
   uint32                    width;            /* window width */
   uint32                    height;           /* window height */
   f32                       r, g, b;          /* window background color */
   struct _MASS_UI_WIN       *children;        /* children windows */
} MASS_UI_WIN;

MASS_UI_WIN             *windows;

int init() {

   MASS_UI_WIN       *w;

   w = (MASS_UI_WIN*)malloc(sizeof(MASS_UI_WIN) * 4);

   memset(w, 0, sizeof(MASS_UI_WIN) * 4);

   w[0].r = 1.0;
   w[0].g = 0.0;
   w[0].b = 0.0;
   w[0].height = 290;
   w[0].width = 290;
   w[0].top = 0;
   w[0].left = 0;

   mass_ll_add((void**)&windows, &w[0]);

   w[1].r = 0.0;
   w[1].g = 1.0;
   w[1].b = 0.0;
   w[1].height = 100;
   w[1].width = 300;
   w[1].top = 10;
   w[1].left = 10;

   mass_ll_add((void**)&w[0].children, &w[1]);
   return 1;
}

void drawUI(MASS_UI_WIN *windows, f64 gx, f64 gy, f64 gw, f64 gh, f64 fpw, f64 fph) {
   f64         ax, ay, aw, ah;

   /* draw the windows in order of link list */
   for (MASS_UI_WIN *cw = windows; cw; cw = (MASS_UI_WIN*)mass_ll_next(cw)) {
      glColor3f(cw->r, cw->g, cw->b);

      /* calculate absolute -1.0 to 1.0 coordinates with parent offset */
      ax = gx + cw->left * fpw;
      ay = gy + cw->top * fph;
      aw = ax + cw->width * fpw;
      ah = ay + cw->height * fph;

      /* if it is outside of parent space just dont draw any of it */
      if (ax > gw)
         continue;
      if (ay > gh)
         continue;

      /* if it extends outside of parent only draw that which reaches */
      if (aw > gw)
         aw = gw;
      if (ah > gh)
         ah = gh;

      /* draw the actual window */
      glBegin(GL_QUADS);
      glVertex2d(ax, -ay);
      glVertex2d(aw, -ay);
      glVertex2d(aw, -ah);
      glVertex2d(ax, -ah);
      glEnd();

      drawUI(cw->children, ax, ay, aw, ah, fpw, fph);
   }
}

void display() {
   int         w, h;

   w = glutGet(GLUT_WINDOW_WIDTH);
   h = glutGet(GLUT_WINDOW_HEIGHT);

   gluPerspective(0.5, 1.0, 0.1, 1000.0);

   glLoadIdentity();

   glPointSize(5.0);
   glColor3d(1.0, 1.0, 1.0);
   glBegin(GL_POINTS);
   glVertex3d(0.0, 0.0, 1.0);
   glEnd();

   gluOrtho2D(0.0, 0.0, w, h);

   drawUI(windows, -1.0, -1.0, 1.0, 1.0, 2.0 / w, 2.0 / h);

   glutSwapBuffers();
}

void idle() {
   glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
   printf("key:%c x:%i y:%i\n", key, x, y);
}

void mouse(int button, int state, int x, int y) {
}

DWORD WINAPI mass_client_entry(void *arg) {
   // initialize Lua
   // initialize OpenGL
   // implement support for Lua window drawing to create the UI
   // Lua scripts run

   MASS_CLIENT_ARGS        *args;

   args = (MASS_CLIENT_ARGS*)arg;

   glutInit(&args->argc, args->argv);
   glutInitWindowPosition(50, 50);
   glutInitWindowSize(300, 300);
   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
   glutCreateWindow("Mass");

   glutDisplayFunc(display);
   glutKeyboardFunc(keyboard);
   glutMouseFunc(mouse);
   glutIdleFunc(idle);

   if (!init())
      return 1;

   glutMainLoop();
   return 0;
}