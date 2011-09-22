#include "lua51_sxc.h"

int luaopen_lua51_sxc(lua_State *L) {
  printf("in luaopen\n");

  /* create require_sxc function */
  lua_pushlightuserdata(L, sxc_load);
  lua_pushcclosure(L, l_sxc_invoke, 1);
  lua_setglobal(L, "require_sxc");

  return 0;
}


int l_sxc_invoke(lua_State* L) {
  SxcContext context;
  const int argcount = lua_gettop(L);
  SxcApi api = (SxcApi)lua_touserdata(L, lua_upvalueindex(1));
  SxcValue* return_value = sxc_context_try(&context, &CONTEXT_METHODS, L, argcount, api);
  const int garbage = lua_gettop(L) - argcount;

printf("in l_sxc_invoke, argcount: %d, garbage: %d\n", argcount, garbage);

  push_value(&context, return_value);
  if (garbage > 0) {
    /* NOTE after this point no SxcStrings, SxcMaps, or SxcFunctions are valid,
    because their index into the stack may be one replaced/popped below */

printf("top: %d, type type: %s, arg+1 type: %s\n", lua_gettop(L), lua_typename(L, lua_type(L, -1)), lua_typename(L, lua_type(L, argcount + 1)));

    lua_replace(L, argcount + 1);

printf("retval type: %s\n", lua_typename(L, lua_type(L, argcount + 1)));

    lua_pop(L, garbage - 1/* lua_replace pops top element */);

printf("post-pop top: %d, top type: %s\n", lua_gettop(L), lua_typename(L, lua_type(L, -1)));
  }
  sxc_context_finally(&context);

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
    return 0;
  }

printf("done check t[0]\n");

  /* AND if t[1] is defined, we assume t is a list. */
  lua_pushinteger(L, 1);
  lua_gettable(L, index);
  type = lua_type(L, -1);
  lua_pop(L, 1);
  if (type != LUA_TNIL) {
    return 1;
  }

printf("done check t[1]\n");

  /* We also say empty tables are probably lists (so that if the first keys
      added are integer keys, they will be adjusted to 1-based indexing). */
  lua_pushnil(L);
  if (!lua_next(L, index)) {
    return -1;
  }
  lua_pop(L, 2);

printf("done check empty\n");

  return 0;
}


SxcString* get_string(SxcContext* context, int index) {
  lua_State* L = (lua_State*)context->underlying;
  SxcString* s = sxc_context_alloc(context, sizeof(SxcString) + sizeof(int));
  size_t length;

  if (index < 0) {
    index += lua_gettop(L) + 1;
  }

  s->context = context;
  s->underlying = s + 1;
  *(int*)(s->underlying) = index;
  s->data = (char*)lua_tolstring(L, index, &length);
  s->length = (int)length;
  return s;
}


SxcMap* get_map(SxcContext* context, int index) {
  lua_State* L = (lua_State*)context->underlying;
  SxcMap* m = sxc_context_alloc(context, sizeof(SxcMap) + sizeof(int));

  if (index < 0) {
    index += lua_gettop(L) + 1;
  }

  m->context = context;
  m->methods = &MAP_METHODS;
  m->underlying = m + 1;
  *(int*)(m->underlying) = index;
  m->is_list = table_is_list(L, index);
  return m;
}


SxcFunction* get_function(SxcContext* context, int index) {
  lua_State* L = (lua_State*)context->underlying;
  SxcFunction* f = sxc_context_alloc(context, sizeof(SxcFunction) + sizeof(int));

  if (index < 0) {
    index += lua_gettop(L) + 1;
  }

  f->context = context;
  f->methods = &FUNCTION_METHODS;
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
      sxc_value_set_null(return_value);
      return;

    case LUA_TBOOLEAN:
      sxc_value_set_boolean(return_value, (char)lua_toboolean(L, index));
      return;

    case LUA_TNUMBER:
      number = (double)lua_tonumber(L, index);
      if (number == (double)(int)number) {
        sxc_value_set_integer(return_value, (int)number);
      } else {
        sxc_value_set_decimal(return_value, number);
      }
      return;

    case LUA_TSTRING:
      sxc_value_set_string(return_value, get_string(context, index));
      return;

    case LUA_TTABLE:
      sxc_value_set_map(return_value, get_map(context, index));
      return;

    case LUA_TFUNCTION:
      sxc_value_set_function(return_value, get_function(context, index));
      return;
  }
}


void push_value(SxcContext* context, SxcValue* value) {
  lua_State* L = (lua_State*)context->underlying;

  switch (value->type) {
    default:
    case sxc_type_null:
      lua_pushnil(L);
      return;

    case sxc_type_boolean:
      lua_pushboolean(L, sxc_value_get_boolean(value, 0));
      return;

    case sxc_type_integer:
      lua_pushinteger(L, (lua_Integer)sxc_value_get_integer(value, 0));
      return;

    case sxc_type_decimal:
      lua_pushnumber(L, (lua_Number)sxc_value_get_decimal(value, 0.0));
      return;

    case sxc_type_string:
      lua_pushvalue(L, *(int*)(sxc_value_get_string(value, NULL)->underlying));
      return;

    case sxc_type_map:
      lua_pushvalue(L, *(int*)(sxc_value_get_map(value, NULL)->underlying));
      return;

    case sxc_type_function:
      lua_pushvalue(L, *(int*)(sxc_value_get_function(value, NULL)->underlying));
      return;
  }
}
