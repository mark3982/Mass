#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "linklist.h"
#include "client.h"
#include "cl_lua.h"

/*
   There are three code layers to window management.

   1. Internal C/C++ Management
      Handles rendering the windows, text, textures, ..etc.
   2. Internal C/C++ Callback
      This handles mainly converting the window events into Lua 
      calls so that Lua can handle events. It also checks if
      a window can be dragged before sending Lua the event, and
      it actually performs the window drag.
   3. External Lua Callback 
*/

static MASS_UI_WIN             *focus;
static MASS_UI_WIN             *windows;
static void                    *texdata;

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

/* TODO: add code to check if 'name' already exists */
GLuint mass_ui_texman_memload(void *texdata, int dim, uint8 *name)
{
   GLuint                  nref;
   MASS_UI_TEXMAN_TEX      *ct;

   ct = (MASS_UI_TEXMAN_TEX*)malloc(sizeof(MASS_UI_TEXMAN_TEX));

   glGenTextures(1, &nref);
   glBindTexture(GL_TEXTURE_2D, nref);
   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   // GL_REPEAT
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   gluBuild2DMipmaps(GL_TEXTURE_2D, 4, dim, dim, GL_RGBA, GL_UNSIGNED_BYTE, texdata);

   ct->gltexref = nref;
   ct->path = name;
   mass_ll_add((void**)&textures, ct);

   return nref;
}

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

   ct->path = path;

   fseek(fp, 0, SEEK_END);
   fsz = ftell(fp);
   texdata = malloc(fsz);
   fseek(fp, 0, SEEK_SET);
   fread(texdata, fsz, 1, fp);
   fclose(fp);

   /* i only produce raw images squared (256x256,512x512,..etc) and 32-bit pixel color in RGBA format */
   dim = (uint32)sqrt((f64)fsz / 4.0);

   return mass_ui_texman_memload(texdata, dim, path);   
}

void unknown() {
   uint32      *img;
   int         x, y;
   uint32      color1;

   // bgra
   img = (uint32*)malloc(sizeof(uint32) * 256 * 256);

   //img[x + y * 256]
   //c8d0d4ff
   //000000ff

   /* paint everything white */
   for (x = 0; x < 256; ++x) {
      for (y = 0; y < 256; ++y) {
         img[x + y * 256] = 0xffffffff;
      }
   }

   color1 = 0xc8d0d4ff;
   color1 = 0xffc8d0d4;

   /* paint border lines */
   for (x = 0, y = 0; x < 256; ++x) {
      img[x + y * 256] = color1;
   }
   for (x = 0, y = 255; x < 256; ++x) {
      img[x + y * 256] = color1;
   }
   for (x = 0, y = 0; y < 256; ++y) {
      img[x + y * 256] = color1;
   }
   for (x = 255, y = 0; y < 256; ++y) {
      img[x + y * 256] = color1;
   }

   /* paint center area */
   for (y = 2; y < 254; ++y) {
      for (x = 2; x < 254; ++x) {
         img[x + y * 256] = color1;
      }
   }

   mass_ui_texman_memload(img, 256, (uint8*)"$def");
}

static int init() {
   //MASS_UI_WIN       *w;

   //char              cwd[1024];
   //GetCurrentDirectoryA(1024, &cwd[0]);

   //w = (MASS_UI_WIN*)malloc(sizeof(MASS_UI_WIN) * 4);

   //memset(w, 0, sizeof(MASS_UI_WIN) * 4);

   /*
   w[0].flags |= MASS_UI_DRAGGABLE;
   w[0].r = 1.0;
   w[0].g = 1.0;
   w[0].b = 1.0;
   w[0].height = 300;
   w[0].width = 300;
   w[0].top = 0;
   w[0].left = 0;
   w[0].text = (uint16*)L"hello world";
   w[0].tr = 0.0;
   w[0].tg = 0.0;
   w[0].tb = 0.0;
   w[0].bgimgpath = (uint8*)"$def";
   w[0].cb = defcb;

   mass_ll_add((void**)&windows, &w[0]);

   w[1].flags |= MASS_UI_DRAGGABLE;
   w[1].r = 1.0;
   w[1].g = 1.0;
   w[1].b = 1.0;
   w[1].height = 100;
   w[1].width = 170;
   w[1].top = 10;
   w[1].left = 10;
   w[1].cb = defcb;
   w[1].bgimgpath = (uint8*)"$def";
   // TODO: fix problem with 'text' being null/zero
   w[1].text = (uint16*)L"small window";
   w[1].parent = &w[0];

   mass_ll_add((void**)&w[0].children, &w[1]);
   */
   return 1;
}

/*
   This handles determining what window was clicked and where.
*/
static int mass_ui_click(MASS_UI_WIN *windows, int32 x, int32 y, MASS_UI_WIN **clicked, uint32 *lx, uint32 *ly) {
   *clicked = 0;
   *lx = 0;
   *ly = 0;
   
   for (MASS_UI_WIN *cw = (MASS_UI_WIN*)mass_ll_last(windows); cw; cw = (MASS_UI_WIN*)mass_ll_prev(cw)) {
   //for (MASS_UI_WIN *cw = windows; cw; cw = (MASS_UI_WIN*)mass_ll_next(cw)) {
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

/*
   This handles (actually) events from both the keyboard AND the mouse. It
   does not handle mouse move events at the moment. At the time of this
   writting they are not implemented.
*/
static void _mouse(int button, int state, int x, int y) {
   MASS_UI_WIN          *clicked;
   uint32               lx, ly;
   MASS_UI_EVTINPUT     evt;
   uint8                key;
   f64                  delta;
   MASS_UI_DRAG         devt;

   /* feels terrible.. but.. it works */
   static MASS_UI_WIN   *lastClicked = 0;
   static uint32        lastX = 0, lastY = 0;
   static uint32        buttons = 0;

   /* it comes in backwards */
   state = !state;

   //printf("button:%i state:%i x:%i y:%i\n", button, state, x, y);
   mass_ui_click(windows, x, y, &clicked, &lx, &ly);

   /* if button is released (telling it is it was previously pushed)
      also check if lastClicked is valid and check that the cb has
      been set to a hopefully valid address
   */
   if (state == 0 && lastClicked && lastClicked->cb) {
      delta = sqrt(pow((f64)lastX - (f64)x, 2.0) + pow((f64)lastY - (f64)y, 2.0));
      if (delta > 3.0) {
         devt.key = button;
         devt.from = lastClicked;
         devt.to = clicked;
         devt.lx = lastX;
         devt.ly = lastY;
         devt.dx = x - lastX;
         devt.dy = y - lastY;
         lastClicked->cb(lastClicked, MASS_UI_TY_EVTDRAG, &devt);
      } 
   }

   /* we only need to set on a push/press */
   if (state == 1) {
      lastClicked = clicked;
      lastX = x;
      lastY = y;
   }
   /* 
      (talking about above code blocks)
      we also need to do this before the next condition 
      because a drag could end up on the screen and no
      window, so.. see below.
   */


   /* if no window clicked just exit */
   if (!clicked)
      return;

   if (clicked->cb) {
      switch (button) {
         case 1:
            key = MASS_UI_IN_A;
            break;
         case 2:
            key = MASS_UI_IN_B;
            break;
         /* if the button is unknown then just ignore it */
         default:
            printf("unknown button ignored\n");
            return;
      }

      evt.key = key;
      /* state is inverted; 0 = pushed and 1 = released which is backwards */
      evt.pushed = state; /* correct state 0 == released and 1 == pushed */
      evt.ptrx = x;
      evt.ptry = y;

      /* if main button was pressed (A on Xbox or left mouse button) */
      if ((evt.key & MASS_UI_IN_A) && (evt.pushed != 0)) {
         /* bring window stack to top level on each tier while respecting flags */
         for (MASS_UI_WIN *cw = clicked; cw; cw = cw->parent) {
            if ((cw->flags & MASS_UI_NOTOP) == 0) {
               if (cw->parent) {
                  /* put window on top of stack (top level) */
                  mass_ll_rem((void**)&cw->parent->children, cw);
                  mass_ll_addLast((void**)&cw->parent->children, cw);
               } else {
                  /* if no parent it must be a top level window */
                  mass_ll_rem((void**)&windows, cw);
                  /* last in chain is last rendered (and appears on top of other windows) */
                  mass_ll_addLast((void**)&windows, cw);
               }
            }
         }

         /* if window can not accept focus */
         if (!clicked || (clicked->flags & MASS_UI_NOFOCUS)) {
            /* focus = 0; -- just do not change the focus */
         } else {
            focus = clicked;
         }
      }

      evt.focus = focus;

      clicked->cb(clicked, MASS_UI_TY_EVTINPUT, &evt);
   }
}

/*
   This is the entry point for mouse events from GLUT.
*/
static void mouse(int button, int state, int x, int y) {
   /* we need to map the buttons to another configuration like a controller */
   switch (button) {
      case 0:
         _mouse(MASS_UI_IN_A, state, x, y);
         break;
      case 2:
         _mouse(MASS_UI_IN_B, state, x, y);
         break;
      /* if the button is unknown then just ignore it */
      default:
         return;
   }
}

/*
   A recursively called function capable of drawing the UI.
*/
static void mass_ui_draw(MASS_UI_WIN *windows, f64 gx, f64 gy, f64 gw, f64 gh, f64 fpw, f64 fph) {
   f64         ax, ay, aw, ah;
   f64         cgw, cgh;        /* changed width and height */

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

      /* if it extends outside of parent then clip it (but keep in mind clipping textures) */
      cgw = 1.0; /* default */
      if (aw > gw) {
         cgw = ((aw - gw) / fpw) / cw->width;
         cgw = 1.0 - cgw;
         //cgw = 1.0 - (aw - gw);  /* get pixel width delta */
         aw = gw;
         //cgw = cgw * .99;
      }

      cgh = 1.0; /* default */
      if (ah > gh) { 
         cgh = ((ah - gh) / fph) / cw->height;
         cgh = 1.0 - cgh;
         ah = gh;
         //cgh = cgh * .99;
      }

      if (cw->bgimgpath) {
         /* if it can not load it the GLuint should be 0 */
         glEnable(GL_TEXTURE_2D);
         glBindTexture(GL_TEXTURE_2D, mass_ui_texman_diskload(cw->bgimgpath));
      } else {
         glDisable(GL_TEXTURE_2D);
      }

      /* draw the actual window */
      glBegin(GL_QUADS);
      glTexCoord2d(0.0, 0.0);   // 0, 0
      glVertex2d(ax, -ay);
      glTexCoord2d(cgw, 0.0);   // 1, 0
      glVertex2d(aw, -ay);
      glTexCoord2d(cgw, cgh);   // 1, 1
      glVertex2d(aw, -ah);
      glTexCoord2d(0.0, cgh);   // 0, 1
      glVertex2d(ax, -ah);
      glEnd();

      if (cw->text) {
         glColor3f(cw->tr, cw->tg, cw->tb);
         glRasterPos2d(ax, -ay - 8.0 * fph);

         for (int32 i = 0, x = 0; cw->text[x] != 0 && i < cw->width - 8; ++x, i += 8) {
            glutBitmapCharacter(GLUT_BITMAP_8_BY_13, cw->text[x]);
         }
      }
      mass_ui_draw(cw->children, ax, ay, aw, ah, fpw, fph);
   }
}

static void display() {
   int         w, h;

   /* initialize */
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
   gluPerspective(0.5, 1.0, 0.1, 1000.0);
   glLoadIdentity();

   /* draw */
   glPointSize(5.0);
   glColor3d(1.0, 1.0, 1.0);
   glBegin(GL_POINTS);
   glVertex3d(0.0, 0.0, 1.0);
   glEnd();

   /*
      The call starts the draw operation of the UI. This is
      where the UI drawing code starts and ends.
   */
   w = glutGet(GLUT_WINDOW_WIDTH);
   h = glutGet(GLUT_WINDOW_HEIGHT);
   gluOrtho2D(0.0, 0.0, w, h);
   mass_ui_draw(windows, -1.0, -1.0, 0.8, 0.8, 2.0 / w, 2.0 / h);

   glutSwapBuffers();
}

/*
   This function is called every 40/1000 milliseconds
   to hopefully produce roughly 40 fps. This is the 
   only obvious way I can think of at this time. If
   something better is needed then it can be improved
   later.
*/
static void idle(int interval) {
   glutPostRedisplay();
   glutTimerFunc(1000 / 40, idle, 0);
   /* also let Lua do anything it needs per render tick */
   mass_cl_luaRenderTick();
}

/*
   This is the keymap of the keyboard and the
   XBox controller. It could be any controller
   even a PS3 one. I just wanted to go ahead
   and have this layer of abstraction if it does
   turn out possible to get this to run on Xbox
   or the PS.
*/
static uint16 mass_ui_whatbtn(unsigned char key) {
   switch (key) {
      case 'x': return MASS_UI_IN_X; 
      case 'y': return MASS_UI_IN_Y;
      case 'k': return MASS_UI_IN_LS;
      case 'l': return MASS_UI_IN_RS;
      case 'm': return MASS_UI_IN_LB;
      case ',': return MASS_UI_IN_RB;
      case 'o': return MASS_UI_IN_LT;
      case 'p': return MASS_UI_IN_RT;
      default: return 0;
   }
}

/*
   At the moment the _mouse works as the catch all.
   It could prolly be named differently but I have not got
   around to messing with it. And, also mouse events
   go through this _mouse procedure.
*/
static void keyboardup(unsigned char key, int x, int y) {
   _mouse(mass_ui_whatbtn(key), 0, x, y);
}
static void keyboard(unsigned char key, int x, int y) {
   _mouse(mass_ui_whatbtn(key), 1, x, y);
}

/*
   The C UI to Lua interface.
*/
int mass_lua_winset_fgcolor(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->tr = (f32)lua_tonumber(lua, 2);
   win->tg = (f32)lua_tonumber(lua, 3);
   win->tb = (f32)lua_tonumber(lua, 4);

   return 0;
}
int mass_lua_winset_bgimg(lua_State *lua) {
   MASS_UI_WIN    *win;
   uint8          *bgimg;
   const char     *tmp;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   tmp = lua_tostring(lua, 2);

   bgimg = (uint8*)malloc(strlen(tmp));
   strcpy((char*)bgimg, tmp);

   /* if already set clear it and free the string */
   if (win->bgimgpath)
      free(win->bgimgpath);

   win->bgimgpath = bgimg;
   return 0;
}
int mass_lua_winset_flags(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->flags = (uint32)lua_tonumber(lua, 2);

   return 0;
}
int mass_lua_winset_location(lua_State *lua) {
   MASS_UI_WIN    *win;
   uint32         x, y;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);
   x = (uint32)lua_tonumber(lua, 2);
   y = (uint32)lua_tonumber(lua, 3);

   win->left = x;
   win->top = y;

   return 0;
}
int mass_lua_winset_width(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->width = (int32)lua_tonumber(lua, 2);

   return 0;
}
int mass_lua_winset_height(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->height = (int32)lua_tonumber(lua, 2);

   return 0;
}
int mass_lua_winset_bgcolor(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->r = (f32)lua_tonumber(lua, 2);
   win->g = (f32)lua_tonumber(lua, 3);
   win->b = (f32)lua_tonumber(lua, 4);

   return 0;
}
int mass_lua_winset_text(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   
   win->text = (uint16*)lua_tostring(lua, 2);

   return 0;
}
int mass_lua_destroywindow(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   if (win->parent)
      mass_ll_rem((void**)&win->parent->children, win);
   else
      mass_ll_rem((void**)&windows, win);
   return 0;
}
int mass_lua_createwindow(lua_State *lua) {
   MASS_UI_WIN    *win;
   MASS_UI_WIN    *parent;

   win = (MASS_UI_WIN*)malloc(sizeof(MASS_UI_WIN));
   memset(win, 0, sizeof(MASS_UI_WIN));

   parent = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   /* set the Lua interface callback */
   win->cb = mass_cl_luacb;
   win->parent = parent;

   /* either add to top level or as child */
   if (!parent)
      mass_ll_addLast((void**)&windows, win);
   else
      mass_ll_addLast((void**)&parent->children, win);

   lua_pushlightuserdata(lua, win);
   return 1;
}
int mass_lua_scrdim(lua_State *lua) {
   int      w;
   int      h;

   w = glutGet(GLUT_WINDOW_WIDTH);
   h = glutGet(GLUT_WINDOW_HEIGHT);
   
   lua_pushnumber(lua, w);
   lua_pushnumber(lua, h);
   return 2;
}

/*
   The client entry point.
*/
DWORD WINAPI mass_client_entry(void *arg) {
   MASS_CLIENT_ARGS        *args;
   int                     er;

   args = (MASS_CLIENT_ARGS*)arg;

   glutInit(&args->argc, args->argv);
   glutInitWindowPosition(50, 50);
   glutInitWindowSize(300, 300);
   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
   glutCreateWindow("Mass");

   mass_cl_luainit();

   glutDisplayFunc(display);
   glutKeyboardFunc(keyboard);
   glutMouseFunc(mouse);
   glutTimerFunc(1000 / 40, idle, 0);

   if (!init())
      return 1;

   /*
      Here I initialize some pre-generted textures and set the focus to zero.
   */
   unknown();  /* generate textures */
   focus = 0;

   glutMainLoop();
   return 0;
}