#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "linklist.h"
#include "client.h"

MASS_UI_WIN             *windows;
void                    *texdata;

/*
   The texman primary job is to make sure textures
   are not loaded more than once and to do this it
   has control over texture loading.
*/
typedef struct MASS_UI_TEXMAN_TEX {
   MASS_LL_HDR    llhdr;
   uint8          *path;
   GLuint         gltexref;
} MASS_UI_TEXMAN_TEX;

MASS_UI_TEXMAN_TEX      *textures;   /* internal to texman */

GLuint mass_ui_texman_diskload(uint8 *path) {
   FILE                 *fp;
   uint32               fsz;
   GLuint               nref;
   void                 *texdata;
   MASS_UI_TEXMAN_TEX   *ct;
   uint32               dim;

   /* see if texture is already in memory */
   for (ct = textures; ct; ct = (MASS_UI_TEXMAN_TEX*)mass_ll_next(ct)) {
      if (strcmp((char*)ct->path, (char*)path) == 0) {
         return ct->gltexref;
      }
   }

   fp = fopen((char*)path, "rb");
   if (!fp) {
      return 0;
   }

   ct = (MASS_UI_TEXMAN_TEX*)malloc(sizeof(MASS_UI_TEXMAN_TEX));
   ct->path = path;

   fseek(fp, 0, SEEK_END);
   fsz = ftell(fp);
   texdata = malloc(fsz);
   fseek(fp, 0, SEEK_SET);
   fread(texdata, fsz, 1, fp);
   fclose(fp);

   /* i only produce raw images squared (256x256,512x512,..etc) and 32-bit pixel color in RGBA format */
   dim = sqrt((f64)fsz / 4.0);

   glGenTextures(1, &nref);
   glBindTexture(GL_TEXTURE_2D, nref);
   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   gluBuild2DMipmaps(GL_TEXTURE_2D, 4, dim, dim, GL_RGBA, GL_UNSIGNED_BYTE, texdata);
   free(texdata);

   ct->gltexref = nref;
   mass_ll_add((void**)&textures, ct);

   return nref;
}

void defcb(MASS_UI_WIN *win, uint32 evtype, void *ev) {
   MASS_UI_EVTINPUT        *evi;

   switch (evtype) {
      case MASS_UI_TY_EVTINPUT:
         evi = (MASS_UI_EVTINPUT*)ev;
         printf("key:%u pushed:%u x:%u y:%u\n", evi->key, evi->pushed, evi->ptrx, evi->ptry);
         win->r = RANDFP();
         win->g = RANDFP();
         win->b = RANDFP();
         win->tr = RANDFP();
         win->tg = RANDFP();
         win->tb = RANDFP();
         break;
   }
}

int init() {

   MASS_UI_WIN       *w;
   FILE              *fp;
   //char              cwd[1024];
   GLuint            tex1;
   uint32            fsz;

   //GetCurrentDirectoryA(1024, &cwd[0]);

   w = (MASS_UI_WIN*)malloc(sizeof(MASS_UI_WIN) * 4);

   memset(w, 0, sizeof(MASS_UI_WIN) * 4);

   w[0].r = 1.0;
   w[0].g = 1.0;
   w[0].b = 1.0;
   w[0].height = 290;
   w[0].width = 290;
   w[0].top = 0;
   w[0].left = 0;
   w[0].text = (uint16*)L"hello world";
   w[0].tr = 0.0;
   w[0].tg = 0.0;
   w[0].tb = 0.0;
   w[0].bgimgpath = (uint8*)"texture.raw";
   w[0].cb = defcb;

   mass_ll_add((void**)&windows, &w[0]);

   w[1].r = 0.0;
   w[1].g = 1.0;
   w[1].b = 0.0;
   w[1].height = 100;
   w[1].width = 170;
   w[1].top = 10;
   w[1].left = 10;
   w[1].text = (uint16*)L"small window";

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
int mass_ui_click(MASS_UI_WIN *windows, uint32 x, uint32 y, MASS_UI_WIN **clicked, uint32 *lx, uint32 *ly) {
   *clicked = 0;
   *lx = 0;
   *ly = 0;
   for (MASS_UI_WIN *cw = windows; cw; cw = (MASS_UI_WIN*)mass_ll_next(cw)) {
      if (((cw->left) < x) && (x < (cw->left + cw->width)))
         if (((cw->top) < y) && (y < (cw->top + cw->height))) {
            /* click landed on this window and no children or nopassclick */
            if (!cw->children || (cw->flags & MASS_UI_NOPASSCLICK)) {
               *lx = x;
               *ly = y;
               *clicked = cw;
               return 1;
            }
            /* 
               go deeper and determine where the click landed; i set the pointer
               here because
            */
            switch (mass_ui_click(cw->children, x - cw->left, y - cw->top, clicked, lx, ly)) {
               case 1:
                  /* rapid exit; clicked has been set; and we need to unwind the call stack */
                  return 1;
               case 0:
                  /* no children were clicked on; set clicked; do rapid exit */
                  *lx = x;
                  *ly = y;
                  *clicked = cw;
                  return 1;
            }
         }
   }
   return 0;
}

void mouse(int button, int state, int x, int y) {
   MASS_UI_WIN       *clicked;
   uint32            lx, ly;
   MASS_UI_EVTINPUT  evt;
   uint8             key;

   //printf("button:%i state:%i x:%i y:%i\n", button, state, x, y);
   mass_ui_click(windows, x, y, &clicked, &lx, &ly);

   /* if no window clicked just exit */
   if (!clicked)
      return;

   if (clicked->cb) {
      switch (button) {
         case 0:
            key = MASS_UI_IN_A;
            break;
         case 2:
            key = MASS_UI_IN_B;
            break;
         /* if the button is unknown then just ignore it */
         default:
            return;
      }

      evt.key = key;
      evt.pushed = state;
      evt.ptrx = x;
      evt.ptry = y;

      clicked->cb(clicked, MASS_UI_TY_EVTINPUT, &evt);
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

      if (cw->bgimgpath) {
         /* if it can not load it the GLuint should be 0 */
         glEnable(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, mass_ui_texman_diskload(cw->bgimgpath));
      } else {
         glDisable(GL_TEXTURE_2D);
      }

      /* draw the actual window */
      glBegin(GL_QUADS);
      glTexCoord2d(0.0, 0.0);   // 0, 1
      glVertex2d(ax, -ay);
      glTexCoord2d(1.0, 0.0);   // 1, 1
      glVertex2d(aw, -ay);
      glTexCoord2d(1.0, 1.0);   // 1, 0
      glVertex2d(aw, -ah);
      glTexCoord2d(0.0, 1.0);   // 0, 0
      glVertex2d(ax, -ah);
      glEnd();


      glColor3f(cw->tr, cw->tg, cw->tb);
      glRasterPos2d(ax, -ay - 8.0 * fph);

      for (int i = 0, x = 0; cw->text[x] != 0 && i < cw->width - 8; ++x, i += 8) {
         glutBitmapCharacter(GLUT_BITMAP_8_BY_13, cw->text[x]);
      }

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