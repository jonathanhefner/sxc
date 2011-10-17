#include "lua51_sxc.h"


static void map_intget(SxcMap* map, int key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;
  key += 1; /* adjust for 1-based indexing */

  lua_pushinteger(L, (lua_Integer)key);
  luaL_checkstack(L, 1 + 2, "");
  lua_gettable(L, *(int*)map->underlying);
  pop_value(map->context, return_value);
}


static void map_intset(SxcMap* map, int key, SxcValue* value) {
  lua_State* L = (lua_State*)map->context->underlying;
  key += 1; /* adjust for 1-based indexing */

  lua_pushinteger(L, (lua_Integer)key);
  push_value(map->context, value);
  lua_settable(L, *(int*)map->underlying);
}


static void map_strget(SxcMap* map, const char* key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;
  luaL_checkstack(L, 1 + 2, "");
  lua_getfield(L, *(int*)map->underlying, key);
  pop_value(map->context, return_value);
}


static void map_strset(SxcMap* map, const char* key, SxcValue* value) {
  lua_State* L = (lua_State*)map->context->underlying;

  /* help deal with 0-based vs 1-based indexing */
  if (map->is_list == TABLE_MAYBE_LIST) {
    map->is_list = TABLE_NOT_LIST;
  }

  push_value(map->context, value);
  lua_setfield(L, *(int*)map->underlying, key);
}


int map_length(SxcMap* map) {
  return -1; /* rely on sxc feature to compute max int key */
}


/* NOTE we assume that sizeof(void*) >= sizeof(int) and use the state pointer
    as a value directly, rather than allocate a structure to hold the current
    index into the stack */
static void* map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;

printf("in map_iter, mapindex: %d, state:%d, statetype:%s\n",
              *(int*)map->underlying, PTR2INT(state), lua_typename(L, lua_type(L, PTR2INT(state))));
  luaL_checkstack(L, 2 + 2, "");

  if (state == NULL) {
    lua_pushnil(L);
  } else if (PTR2INT(state) < lua_gettop(L)) {
    lua_pushvalue(L, PTR2INT(state));
  }

  if (lua_next(L, *(int*)map->underlying)) {
    get_value(map->context, -2, return_key);
    state = INT2PTR(lua_gettop(L) - 2 + 1);

    switch (return_key->type) {
      case sxc_int:
        /* adjust list keys to 0-based indexing */
        return_key->data.aint -= 1;
      case sxc_string:
        pop_value(map->context, return_value);
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
  map_intget, map_intset, map_strget, map_strset, map_length, map_iter
};
