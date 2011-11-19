#include "lua51_sxc.h"


static void map_intget(void* underlying, int key, SxcValue* return_value) {
  lua_State* L = (lua_State*)(return_value->context->underlying);
  key += 1; /* adjust for 1-based indexing */

  lua_pushinteger(L, (lua_Integer)key);
  luaL_checkstack(L, 1 + 2, "");
  lua_gettable(L, PTR2INT(underlying));
  pop_value(return_value);
}


static void map_intset(void* underlying, int key, SxcValue* value) {
  lua_State* L = (lua_State*)(value->context->underlying);
  key += 1; /* adjust for 1-based indexing */

  lua_pushinteger(L, (lua_Integer)key);
  push_value(value);
  lua_settable(L, PTR2INT(underlying));
}


static void map_strget(void* underlying, const char* key, SxcValue* return_value) {
  lua_State* L = (lua_State*)(return_value->context->underlying);

  luaL_checkstack(L, 1 + 2, "");
  lua_getfield(L, PTR2INT(underlying), key);
  pop_value(return_value);
}


static void map_strset(void* underlying, const char* key, SxcValue* value) {
  lua_State* L = (lua_State*)(value->context->underlying);

  push_value(value);
  lua_setfield(L, PTR2INT(underlying), key);
}


static void* map_iter(void* underlying, void* state, SxcValue* return_key, SxcValue* return_value) {
  lua_State* L = (lua_State*)(return_value->context->underlying);

printf("in map_iter, mapindex: %d, state:%d, statetype:%s\n",
              PTR2INT(underlying), PTR2INT(state), lua_typename(L, lua_type(L, PTR2INT(state))));
  luaL_checkstack(L, 2 + 2, "");

  if (state == NULL) {
    lua_pushnil(L);
  } else if (PTR2INT(state) < lua_gettop(L)) {
    lua_pushvalue(L, PTR2INT(state));
  }

  if (lua_next(L, PTR2INT(underlying))) {
    get_value(-2, return_key);
    state = INT2PTR(lua_gettop(L) - 2 + 1);

    switch (return_key->type) {
      case sxc_cint:
        /* adjust list keys to 0-based indexing */
        return_key->data.cint -= 1;
      /* fall through */
      case sxc_sstring:
        pop_value(return_value);
printf("leaving map_iter with new state:%d\n", PTR2INT(state));
        break;

      /* even though sxc already skips non-integer/strings keys, we null out
          invalid pairs so we don't have to leave unused values on the stack */
      default:
        sxc_value_set(return_key, sxc_null);
        sxc_value_set(return_value, sxc_null);
        lua_pop(L, 1);
printf("leaving map_iter nulled with new state:%d\n", PTR2INT(state));
        break;
    }

    return state;
  }

  return NULL;
}


SxcMapBinding MAP_BINDING = {
  map_intget, map_intset, map_strget, map_strset, NULL, map_iter
};
