#include "lua51_sxc.h"


int luaopen_lua51_sxc(lua_State *L) {
printf("in luaopen\n");

  /* create table for map type ctors */
  lua_newtable(L);
    /* add list map type ctor placeholder (so 1st index isn't nil) */
    lua_pushliteral(L, "list ctor placeholder");
    lua_rawseti(L, -2, PTR2INT(MAPTYPE_LIST));
  lua_setfield(L, LUA_REGISTRYINDEX, MAPTYPE_CTORS_KEY);

  /* create require_sxc function */
  lua_pushlightuserdata(L, sxc_load);
  lua_pushcclosure(L, l_libfunc_invoke, 1);
  lua_setglobal(L, "require_sxc");

  return 0;
}


int l_libfunc_invoke(lua_State* L) {
  return libfunc_invoke((SxcLibFunc*)lua_touserdata(L, lua_upvalueindex(1)), L, lua_gettop(L));
}


int libfunc_invoke(SxcLibFunc* func, lua_State* L, const int argcount) {
  SxcContext context;
  const int final_top = lua_gettop(L) + 1;

  sxc_try(&context, L, &CONTEXT_BINDING, argcount, func);
  push_value(&context.return_value);
  if (final_top < lua_gettop(L)) {
    /* NOTE after this point no SxcStrings, SxcMaps, or SxcFuncs are valid,
      because their index into the stack may be one replaced/popped below */
    lua_replace(L, final_top);
    lua_settop(L, final_top);
  }

  sxc_finally(&context);

  /* TODO? prefix error message with "ERROR: " */
  return context.has_error ? lua_error(L) : 1;
}




void get_value(int index, SxcValue* return_value) {
  lua_State* L = (lua_State*)(return_value->context->underlying);
  double number;

  if (index < 0) {
    index += lua_gettop(L) + 1;
  }

  switch (lua_type(L, index)) {
    default:
    case LUA_TNIL:
      sxc_value_set(return_value, sxc_null);
      return;

    case LUA_TBOOLEAN:
      sxc_value_set(return_value, sxc_cbool, (bool)lua_toboolean(L, index));
      return;

    case LUA_TNUMBER:
      number = (double)lua_tonumber(L, index);
      if (number == (double)(int)number) {
        sxc_value_set(return_value, sxc_cint, (int)number);
      } else {
        sxc_value_set(return_value, sxc_cdouble, number);
      }
      return;

    case LUA_TSTRING:
      sxc_value_set(return_value, sxc_sstring, INT2PTR(index), &STRING_BINDING);
      return;

    case LUA_TTABLE:
      sxc_value_set(return_value, sxc_smap, INT2PTR(index), &MAP_BINDING);
      return;

    case LUA_TFUNCTION:
      sxc_value_set(return_value, sxc_sfunc, INT2PTR(index), &FUNC_BINDING);
      return;
  }
}


void pop_value(SxcValue* return_value) {
  get_value(-1, return_value);

  /* pop values that don't need to remain on the stack to use (i.e. primitives) */
  switch (return_value->type) {
    case sxc_null:
      /* NOTE if type is sxc_null, it's because there's a nil value at -1
          (not because get_value() tried to retrieve a non-existent value) */
    case sxc_cbool:
    case sxc_cint:
    case sxc_cdouble:
      lua_pop((lua_State*)(return_value->context->underlying), 1);
      break;

    default:
      break;
  }
}


#include <stdio.h>
void push_value(SxcValue* value) {
  lua_State* L = (lua_State*)(value->context->underlying);

printf("in push_value, type:%d\n", value->type);

  switch (value->type) {
    default:
    case sxc_null:
      lua_pushnil(L);
      return;

    case sxc_cbool:
      lua_pushboolean(L, value->data.cbool);
      return;

    case sxc_cint:
      lua_pushinteger(L, (lua_Integer)value->data.cint);
      return;

    case sxc_cdouble:
      lua_pushnumber(L, (lua_Number)value->data.cdouble);
      return;

    case sxc_sstring:
      lua_pushvalue(L, PTR2INT(value->data.sstring.underlying));
      return;

    case sxc_smap:
      lua_pushvalue(L, PTR2INT(value->data.smap.underlying));
      return;

    case sxc_sfunc:
      lua_pushvalue(L, PTR2INT(value->data.sfunc.underlying));
      return;
  }
}
