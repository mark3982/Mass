#include <lua.h>
#include <stdio.h>
#include <lualib.h>
#include <lauxlib.h>

#include "client.h"
#include "cl_lua.h"

/* I know it is bad, but I had too for the moment.. */
lua_State               *g_lua;

void mass_cl_luainit() {
   lua_State      *lua;
   FILE           *fp;
   int            fsz;
   void           *buf;
   int            er;

   lua = luaL_newstate();
   luaL_openlibs(lua);
   
   fp = fopen(".\\Scripts\\init.lua", "rb");
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
   lua_pushcfunction(lua, mass_lua_winset_location);
   lua_setglobal(lua, "mass_winset_location");
   lua_pushcfunction(lua, mass_lua_winset_flags);
   lua_setglobal(lua, "mass_winset_flags");
   lua_pushcfunction(lua, mass_lua_winset_bgimg);
   lua_setglobal(lua, "mass_winset_bgimg");

   lua_getglobal(lua, "debug");
   lua_getfield(lua, -1, "traceback");
   lua_remove(lua, -2);

   er = luaL_loadbuffer(lua, (char*)buf, fsz, 0);

   /* check for load/compile time errors during loading of chunk */
   if (er) {
      fprintf(stderr, "%s\n", lua_tostring(lua, -1));
      lua_pop(lua, 1);
      printf(".... during loading of init.lua\n");
      exit(-9);
   }

   /* execute chunk */
   er = lua_pcall(lua, 0, 0, 1);

   /* check for runtime errors */
   if (er) {
      fprintf(stderr, "%s", lua_tostring(lua, -1));
      lua_pop(lua, 1);
      exit(-9);
   }

   /* pop error handler */
   lua_pop(lua, 1);
   
   g_lua = lua;
}

/*
   The callback handler.
*/
void mass_cl_luacb(lua_State *lua, MASS_UI_WIN *win, uint32 evtype, void *ev) {
   MASS_UI_EVTINPUT        *evi;
   MASS_UI_DRAG            *evd;
   int                     er;

   switch (evtype) {
      case MASS_UI_TY_EVTDRAG:
         evd = (MASS_UI_DRAG*)ev;
         
         /* check if the window can be dragged */
         if (!(evd->from->flags & MASS_UI_DRAGGABLE))
            break;

         win->left += evd->dx;
         win->top += evd->dy;

         lua_getglobal(lua, "debug");
         lua_getfield(lua, -1, "traceback");
         lua_remove(lua, -2);

         lua_getglobal(lua, "mass_uievent_evtdrag");
         lua_pushnumber(lua, evd->key);
         lua_pushlightuserdata(lua, evd->from);
         lua_pushlightuserdata(lua, evd->to);
         lua_pushnumber(lua, evd->lx);
         lua_pushnumber(lua, evd->ly);
         lua_pushnumber(lua, evd->dx);
         lua_pushnumber(lua, evd->dy);

         er = lua_pcall(lua, 7, 0, 1);
         
         if (er) {
            fprintf(stderr, "%s\n", lua_tostring(lua, -1));
            lua_pop(lua, 1);  /* pop error message from the stack */
         }

         /* pop error handler */
         lua_pop(lua, 1);
         break;
      case MASS_UI_TY_EVTINPUT:
         evi = (MASS_UI_EVTINPUT*)ev;

         /* push error handler */
         lua_getglobal(lua, "debug");
         lua_getfield(lua, -1, "traceback");
         lua_remove(lua, -2);
         
         /* push function */
         lua_getglobal(lua, "mass_uievent_evtinput");
         /* push arguments in order left to right */
         lua_pushlightuserdata(lua, evi->focus);
         lua_pushlightuserdata(lua, win);
         lua_pushnumber(lua, evi->key);
         lua_pushnumber(lua, evi->pushed);
         lua_pushnumber(lua, evi->ptrx);
         lua_pushnumber(lua, evi->ptry);

         /* execute */
         er = lua_pcall(lua, 6, 0, 1);

         if (er) {
            /* print and pop error message */
            fprintf(stderr, "%s\n", lua_tostring(lua, -1));
            lua_pop(lua, 1);
         }

         /* pop error handler */
         lua_pop(lua, 1);
         break;
   }
}

