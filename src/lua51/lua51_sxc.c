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

  sxc_try(&context, &CONTEXT_BINDING, L, argcount, func);
  push_value(&context, &context.return_value);
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


short int table_is_list(lua_State* L, int index) {
  int type;

printf("in table_is_list\n");

  /* If t[0] is defined, we assume t is not a list */
  lua_pushinteger(L, 0);
  lua_gettable(L, index);
  type = lua_type(L, -1);
  lua_pop(L, 1);
  if (type != LUA_TNIL) {
    return TABLE_NOT_LIST;
  }

printf("done check t[0]\n");

  /* AND if t[1] is defined, we assume t is a list. */
  lua_pushinteger(L, 1);
  lua_gettable(L, index);
  type = lua_type(L, -1);
  lua_pop(L, 1);
  if (type != LUA_TNIL) {
    return TABLE_IS_LIST;
  }

printf("done check t[1]\n");

  /* We also say empty tables are probably lists (so that if the first keys
      added are integer keys, they will be adjusted to 1-based indexing). */
  lua_pushnil(L);
  if (!lua_next(L, index)) {
    return TABLE_MAYBE_LIST;
  }
  lua_pop(L, 2);

printf("done check empty\n");

  return TABLE_NOT_LIST;
}


/* TODO convert `*(int*)(s->underlying) = index` to `s->underlying = INT2PTR(index)` */
SxcString* get_string(SxcContext* context, int index) {
  lua_State* L = (lua_State*)context->underlying;
  SxcString* s = sxc_alloc(context, sizeof(SxcString) + sizeof(int));
  size_t length;

  if (index < 0) {
    index += lua_gettop(L) + 1;
  }

  s->context = context;
  s->underlying = s + 1;
  *(int*)(s->underlying) = index;
  s->data = (char*)lua_tolstring(L, index, &length);
  s->length = (int)length;
  s->is_terminated = TRUE;
  return s;
}


SxcMap* get_map(SxcContext* context, int index, int is_list) {
  lua_State* L = (lua_State*)context->underlying;
  SxcMap* m = sxc_alloc(context, sizeof(SxcMap) + sizeof(int));

  if (index < 0) {
    index += lua_gettop(L) + 1;
  }

  m->context = context;
  m->binding = &MAP_BINDING;
  m->underlying = m + 1;
  *(int*)(m->underlying) = index;
  m->is_list = is_list == TABLE_MAYBE_LIST ? table_is_list(L, index) : is_list;
  return m;
}


SxcFunc* get_func(SxcContext* context, int index) {
  lua_State* L = (lua_State*)context->underlying;
  SxcFunc* f = sxc_alloc(context, sizeof(SxcFunc) + sizeof(int));

  if (index < 0) {
    index += lua_gettop(L) + 1;
  }

  f->context = context;
  f->binding = &FUNC_BINDING;
  f->underlying = f + 1;
  *(int*)(f->underlying) = index;
  return f;
}


void get_value(SxcContext* context, int index, SxcValue* return_value) {
  lua_State* L = (lua_State*)context->underlying;
  double number;

  switch (lua_type(L, index)) {
    default:
    case LUA_TNIL:
      sxc_value_set(return_value, sxc_null);
      return;

    case LUA_TBOOLEAN:
      sxc_value_set(return_value, sxc_bool, (char)lua_toboolean(L, index));
      return;

    case LUA_TNUMBER:
      number = (double)lua_tonumber(L, index);
      if (number == (double)(int)number) {
        sxc_value_set(return_value, sxc_int, (int)number);
      } else {
        sxc_value_set(return_value, sxc_double, number);
      }
      return;

    case LUA_TSTRING:
      sxc_value_set(return_value, sxc_string, get_string(context, index));
      return;

    case LUA_TTABLE:
      sxc_value_set(return_value, sxc_map, get_map(context, index, TABLE_MAYBE_LIST));
      return;

    case LUA_TFUNCTION:
      sxc_value_set(return_value, sxc_func, get_func(context, index));
      return;
  }
}


void pop_value(SxcContext* context, SxcValue* return_value) {
  get_value(context, -1, return_value);

  /* pop values that don't need to remain on the stack to use (i.e. primitives) */
  switch (return_value->type) {
    case sxc_null:
    case sxc_bool:
    case sxc_int:
    case sxc_double:
      lua_pop((lua_State*)context->underlying, 1);
      break;

    default:
      break;
  }
}


#include <stdio.h>
void push_value(SxcContext* context, SxcValue* value) {
  lua_State* L = (lua_State*)context->underlying;

printf("in push_value, type:%d\n", value->type);

  switch (value->type) {
    default:
    case sxc_null:
      lua_pushnil(L);
      return;

    case sxc_bool:
      lua_pushboolean(L, value->data.abool);
      return;

    case sxc_int:
      lua_pushinteger(L, (lua_Integer)value->data.aint);
      return;

    case sxc_double:
      lua_pushnumber(L, (lua_Number)value->data.adouble);
      return;

    case sxc_string:
      lua_pushvalue(L, *(int*)(value->data.string->underlying));
      return;

    case sxc_map:
      lua_pushvalue(L, *(int*)(value->data.map->underlying));
      return;

    case sxc_func:
      lua_pushvalue(L, *(int*)(value->data.func->underlying));
      return;
  }
}
