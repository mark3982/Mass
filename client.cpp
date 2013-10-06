#include <stdio.h>
#include <stdlib.h>
#include <GL\glut.h>

#include "linklist.h"
#include "client.h"

typedef struct _MASS_UI_WIN {
   MASS_LL_HDR               llhdr;            /* link list header */
   uint32                    top;              /* top (y) corner */
   uint32                    left;             /* left (x) corner */
   uint32                    width;            /* window width */
   uint32                    height;           /* window height */
   f32                       r, g, b;          /* window background color */
   struct _MASS_UI_WIN       *children;        /* children windows */
   uint32                    flags;            /* flags */
} MASS_UI_WIN;

MASS_UI_WIN             *windows;
void                    *texdata;

int init() {

   MASS_UI_WIN       *w;
   FILE              *fp;
   char              cwd[1024];
   GLuint            tex1;
   uint32            fsz;

   GetCurrentDirectoryA(1024, &cwd[0]);

   fp = fopen("texture.raw", "rb");
   fseek(fp, 0, SEEK_END);
   fsz = ftell(fp);
   texdata = malloc(fsz);
   fseek(fp, 0, SEEK_SET);
   fread(texdata, fsz, 1, fp);
   fclose(fp);

   /*
   glGenTextures(1, &tex1);
   glBindTexture(GL_TEXTURE_2D, tex1);
   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   gluBuild2DMipmaps(GL_TEXTURE_2D, 3, 256, 256, GL_RGB, GL_UNSIGNED_BYTE, texdata);
   free(texdata);

   glEnable(GL_TEXTURE_2D);
   */

   w = (MASS_UI_WIN*)malloc(sizeof(MASS_UI_WIN) * 4);

   memset(w, 0, sizeof(MASS_UI_WIN) * 4);

   w[0].r = 1.0;
   w[0].g = 1.0;
   w[0].b = 1.0;
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

/*
   Any window with this flag set will not allow clicks to progress
   any further causing mass_ui_click to return this window. 

   A good example is when you are a button and have multiple child
   windows which may be showing text and/or a graphic and you do not
   want to write click handling code for each of these in the event
   they are clicked which will be highly possible. So essentially
   this flag aids the programming writing code for this window system.
*/
#define MASS_UI_NOPASSCLICK            0x01

/*
   This will iterate though the windows and into their children finding ultimately
   which window was clicked.
*/
int mass_ui_click(MASS_UI_WIN *windows, int x, int y, MASS_UI_WIN **clicked) {
   *clicked = 0;
   for (MASS_UI_WIN *cw = windows; cw; cw = (MASS_UI_WIN*)mass_ll_next(cw)) {
      if (((cw->left) < x) && (x < (cw->left + cw->width)))
         if (((cw->top) < y) && (y < (cw->top + cw->height))) {
            /* click landed on this window and no children or nopassclick */
            if (!cw->children || (cw->flags & MASS_UI_NOPASSCLICK)) {
               *clicked = cw;
               return 1;
            }
            /* 
               go deeper and determine where the click landed; i set the pointer
               here because
            */
            switch (mass_ui_click(cw->children, x - cw->left, y - cw->top, clicked)) {
               case 1:
                  /* rapid exit; clicked has been set; and we need to unwind the call stack */
                  return 1;
               case 0:
                  /* no children were clicked on; set clicked; do rapid exit */
                  *clicked = cw;
                  return 1;
            }
         }
   }
   return 0;
}

void mouse(int button, int state, int x, int y) {
   MASS_UI_WIN       *clicked;

   printf("button:%i state:%i x:%i y:%i\n", button, state, x, y);
   mass_ui_click(windows, x, y, &clicked);

   if (clicked) {
      clicked->r = RANDFP();
      clicked->g = RANDFP();
      clicked->b = RANDFP();
   }
}

void mass_ui_draw(MASS_UI_WIN *windows, f64 gx, f64 gy, f64 gw, f64 gh, f64 fpw, f64 fph) {
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
      glTexCoord2d(0.0, 0.0);
      glVertex2d(ax, -ay);
      glTexCoord2d(1.0, 0.0);
      glVertex2d(aw, -ay);
      glTexCoord2d(1.0, 1.0);
      glVertex2d(aw, -ah);
      glTexCoord2d(0.0, 1.0);
      glVertex2d(ax, -ah);
      glEnd();

      mass_ui_draw(cw->children, ax, ay, aw, ah, fpw, fph);
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

   mass_ui_draw(windows, -1.0, -1.0, 1.0, 1.0, 2.0 / w, 2.0 / h);

   glutSwapBuffers();
}

void idle() {
   glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
   printf("key:%c x:%i y:%i\n", key, x, y);
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