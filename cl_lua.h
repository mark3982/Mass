#ifndef _MASS_CL_LUA_H
#define _MASS_CL_LUA_H
#include "core.h"

extern lua_State               *g_lua;

void mass_cl_luacb(lua_State *lua, MASS_UI_WIN *win, uint32 evtype, void *ev);
void mass_cl_luainit();

#endif