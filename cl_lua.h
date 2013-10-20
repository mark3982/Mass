#ifndef _MASS_CL_LUA_H
#define _MASS_CL_LUA_H
#include "core.h"

void mass_cl_luacb(MASS_UI_WIN *win, uint32 evtype, void *ev);
void mass_cl_luainit();
void mass_cl_luaRenderTick();

#endif