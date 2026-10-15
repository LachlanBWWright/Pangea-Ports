#pragma once

typedef struct lua_State lua_State;
void PangeaPresentation_InstallLua(lua_State* lua);
void PangeaPresentation_PushCapabilities(lua_State* lua);
