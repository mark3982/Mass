#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "linklist.h"
#include "client.h"

/* I know it is bad, but I had too for the moment.. */
static lua_State        *g_lua;

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
   dim = (uint32)sqrt((f64)fsz / 4.0);

   glGenTextures(1, &nref);
   glBindTexture(GL_TEXTURE_2D, nref);
   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   // GL_REPEAT
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
   gluBuild2DMipmaps(GL_TEXTURE_2D, 4, dim, dim, GL_RGBA, GL_UNSIGNED_BYTE, texdata);
   free(texdata);

   ct->gltexref = nref;
   mass_ll_add((void**)&textures, ct);

   return nref;
}

/*
   The default callback handler.
*/
static void defcb(lua_State *lua, MASS_UI_WIN *win, uint32 evtype, void *ev) {
   MASS_UI_EVTINPUT        *evi;
   MASS_UI_DRAG            *evd;
   int                     er;

   switch (evtype) {
      case MASS_UI_TY_EVTDRAG:
         evd = (MASS_UI_DRAG*)ev;
         printf("drag from:%x to:%x", evd->from, evd->to);
         printf("lx:%u ly:%u", evd->lx, evd->ly);
         printf("dx:%i dy:%i\n", evd->dx, evd->dy);
         win->left += evd->dx;
         win->top += evd->dy;
         break;
      case MASS_UI_TY_EVTINPUT:
         evi = (MASS_UI_EVTINPUT*)ev;
         printf("key:%u pushed:%u x:%u y:%u\n", evi->key, evi->pushed, evi->ptrx, evi->ptry);
         //win->r = (f32)RANDFP();
         //win->g = (f32)RANDFP();
         //win->b = (f32)RANDFP();
         //win->tr = (f32)RANDFP();
         //win->tg = (f32)RANDFP();
         //win->tb = (f32)RANDFP();

         // push name of function
         lua_getglobal(lua, "mass_uievent");
         // push arguments
         lua_pushlightuserdata(lua, win);
         lua_pushnumber(lua, evi->key);
         lua_pushnumber(lua, evi->pushed);
         lua_pushnumber(lua, evi->ptrx);
         lua_pushnumber(lua, evi->ptry);
         er = lua_pcall(lua, 5, 0, 0);

         if (er) {
            fprintf(stderr, "%s", lua_tostring(lua, -1));
            lua_pop(lua, 1);  /* pop error message from the stack */
            exit(-9);
         }
         break;
   }
}

static int init() {

   MASS_UI_WIN       *w;

   //char              cwd[1024];
   //GetCurrentDirectoryA(1024, &cwd[0]);

   w = (MASS_UI_WIN*)malloc(sizeof(MASS_UI_WIN) * 4);

   memset(w, 0, sizeof(MASS_UI_WIN) * 4);

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
   w[1].cb = defcb;
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

/* actual function handling mouse */
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
         lastClicked->cb(g_lua, lastClicked, MASS_UI_TY_EVTDRAG, &devt);
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
      printf("mouse button:%u\n", button);
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

      clicked->cb(g_lua, clicked, MASS_UI_TY_EVTINPUT, &evt);
   }
}

/* function called by GLUT */
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


      glColor3f(cw->tr, cw->tg, cw->tb);
      glRasterPos2d(ax, -ay - 8.0 * fph);

      for (int32 i = 0, x = 0; cw->text[x] != 0 && i < cw->width - 8; ++x, i += 8) {
         glutBitmapCharacter(GLUT_BITMAP_8_BY_13, cw->text[x]);
      }

      mass_ui_draw(cw->children, ax, ay, aw, ah, fpw, fph);
   }
}

static void display() {
   int         w, h;

   w = glutGet(GLUT_WINDOW_WIDTH);
   h = glutGet(GLUT_WINDOW_HEIGHT);

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

   gluPerspective(0.5, 1.0, 0.1, 1000.0);

   glLoadIdentity();

   glPointSize(5.0);
   glColor3d(1.0, 1.0, 1.0);
   glBegin(GL_POINTS);
   glVertex3d(0.0, 0.0, 1.0);
   glEnd();

   gluOrtho2D(0.0, 0.0, w, h);

   mass_ui_draw(windows, -1.0, -1.0, 0.8, 0.8, 2.0 / w, 2.0 / h);

   glutSwapBuffers();
}

static void idle() {
   glutPostRedisplay();
}

/*
   This is the keymap of the keyboard and the
   XBox controller. It could be any controller
   even a PS3 one.
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

static void keyboardup(unsigned char key, int x, int y) {
   _mouse(mass_ui_whatbtn(key), 0, x, y);
}
static void keyboard(unsigned char key, int x, int y) {
   _mouse(mass_ui_whatbtn(key), 1, x, y);
}

static int mass_lua_winset_fgcolor(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->tr = (f32)lua_tonumber(lua, 2);
   win->tg = (f32)lua_tonumber(lua, 3);
   win->tb = (f32)lua_tonumber(lua, 4);

   return 0;
}

static int mass_lua_winset_width(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->width = (int32)lua_tonumber(lua, 2);

   return 0;
}

static int mass_lua_winset_height(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->height = (int32)lua_tonumber(lua, 2);

   return 0;
}

static int mass_lua_winset_bgcolor(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   win->r = (f32)lua_tonumber(lua, 2);
   win->g = (f32)lua_tonumber(lua, 3);
   win->b = (f32)lua_tonumber(lua, 4);

   return 0;
}

static int mass_lua_winset_text(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)lua_touserdata(lua, 1);

   
   win->text = (uint16*)lua_tostring(lua, 2);

   return 0;
}

static int mass_lua_createwindow(lua_State *lua) {
   MASS_UI_WIN    *win;

   win = (MASS_UI_WIN*)malloc(sizeof(MASS_UI_WIN));
   memset(win, 0, sizeof(MASS_UI_WIN));

   win->cb = defcb;

   mass_ll_addLast((void**)&windows, win);

   lua_pushlightuserdata(lua, win);
   return 1;
}

/*
   global Lua function as 'mass_scrdim'.
*/
static int mass_lua_scrdim(lua_State *lua) {
   int      w;
   int      h;

   w = glutGet(GLUT_WINDOW_WIDTH);
   h = glutGet(GLUT_WINDOW_HEIGHT);
   
   lua_pushnumber(lua, w);
   lua_pushnumber(lua, h);
   return 2;
}

DWORD WINAPI mass_client_entry(void *arg) {
   // initialize Lua
   // initialize OpenGL
   // implement support for Lua window drawing to create the UI
   // Lua scripts run

   MASS_CLIENT_ARGS        *args;
   lua_State               *lua; 
   FILE                    *fp;
   uint32                  fsz;
   void                    *buf;
   int                     er;

   args = (MASS_CLIENT_ARGS*)arg;

   glutInit(&args->argc, args->argv);
   glutInitWindowPosition(50, 50);
   glutInitWindowSize(300, 300);
   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
   glutCreateWindow("Mass");

   lua = luaL_newstate();
   luaL_openlibs(lua);
   
   fp = fopen(".\\Scripts\\main.lua", "rb");
   fseek(fp, 0, SEEK_END);
   fsz = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   
   buf = malloc(fsz);
   fread(buf, fsz, 1, fp);
   fclose(fp);

   lua_pushcfunction(lua, mass_lua_scrdim);
   lua_setglobal(lua, "mass_scrdim");
   lua_pushcfunction(lua, mass_lua_createwindow);
   lua_setglobal(lua, "mass_createwindow");
   lua_pushcfunction(lua, mass_lua_winset_text);
   lua_setglobal(lua, "mass_winset_text");
   lua_pushcfunction(lua, mass_lua_winset_fgcolor);
   lua_setglobal(lua, "mass_winset_fgcolor");
   lua_pushcfunction(lua, mass_lua_winset_bgcolor);
   lua_setglobal(lua, "mass_winset_bgcolor");
   lua_pushcfunction(lua, mass_lua_winset_width);
   lua_setglobal(lua, "mass_winset_width");
   lua_pushcfunction(lua, mass_lua_winset_height);
   lua_setglobal(lua, "mass_winset_height");

   luaL_loadbuffer(lua, (char*)buf, fsz, 0);
   er = lua_pcall(lua, 0, 0, 0);

   if (er) {
      fprintf(stderr, "%s", lua_tostring(lua, -1));
      lua_pop(lua, 1);  /* pop error message from the stack */
      exit(-9);
   }

   g_lua = lua;

   glutDisplayFunc(display);
   glutKeyboardFunc(keyboard);
   glutMouseFunc(mouse);
   glutIdleFunc(idle);

   if (!init())
      return 1;

   glutMainLoop();
   return 0;
}