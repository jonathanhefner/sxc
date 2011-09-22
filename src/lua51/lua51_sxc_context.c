#include "lua51_sxc.h"

static void context_get_arg(SxcContext* context, int index, SxcValue* return_value) {
  get_value(context, index + 1, return_value);
}


static SxcString* string_intern(SxcContext* context, const char* data, int length) {
  lua_State* L = (lua_State*)context->underlying;

  luaL_checkstack(L, 1 + 2, "");
  lua_pushlstring(L, data, length);
  return get_string(context, -1);
}


static SxcMap* map_new(SxcContext* context, short int is_list) {
  lua_State* L = (lua_State*)context->underlying;

  luaL_checkstack(L, 1 + 2, "");
  lua_newtable(L);
  return get_map(context, -1);
}


static SxcFunction* function_wrap(SxcContext* context, SxcApi api) {
  lua_State* L = (lua_State*)context->underlying;

  luaL_checkstack(L, 2 + 2, "");
  lua_pushlightuserdata(L, api);
  lua_pushcclosure(L, l_sxc_invoke, 1);
  return get_function(context, -1);
}


SxcContextMethods CONTEXT_METHODS = {
  context_get_arg, string_intern, map_new, function_wrap
};
