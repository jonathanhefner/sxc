#include "lua51_sxc.h"

static void map_intget(SxcMap* map, int key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;

  /* deal with 0-based vs 1-based indexing */
  if (map->is_list < 0) {
    map->is_list = table_is_list(L, *(int*)map->underlying);
  }
  if (map->is_list) {
    key += 1;
  }

  lua_pushinteger(L, (lua_Integer)key);
  luaL_checkstack(L, 1 + 2, "");
  lua_gettable(L, *(int*)map->underlying);
  get_value(map->context, -1, return_value);
}


static void map_intset(SxcMap* map, int key, SxcValue* value) {
  lua_State* L = (lua_State*)map->context->underlying;

  /* deal with 0-based vs 1-based indexing */
  if (map->is_list < 0) {
    map->is_list = table_is_list(L, *(int*)map->underlying);
    if (map->is_list < 0) {
      map->is_list = key == 0;
    }
  }
  if (map->is_list) {
    key += 1;
  }

  lua_pushinteger(L, (lua_Integer)key);
  push_value(map->context, value);
  lua_settable(L, *(int*)map->underlying);
}


static void map_strget(SxcMap* map, char* key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;
  luaL_checkstack(L, 1 + 2, "");
  lua_getfield(L, *(int*)map->underlying, key);
  get_value(map->context, -1, return_value);
}


static void map_strset(SxcMap* map, char* key, SxcValue* value) {
  lua_State* L = (lua_State*)map->context->underlying;

  /* help deal with 0-based vs 1-based indexing */
  if (map->is_list < 0) {
    map->is_list = 0;
  }

  push_value(map->context, value);
  lua_setfield(L, *(int*)map->underlying, key);
}


/* NOTE we assume that sizeof(void*) >= sizeof(int) and use the state pointer
    as a value directly, rather than allocate a structure to hold the current
    index into the stack */
static void* map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;
  int prev_index = (int)(long int)state;

  if (prev_index == 0) {
    lua_pushnil(L);
  } else {
    lua_pushvalue(L, prev_index);
  }

  /* skip over keys that are not integers or strings, and return NULL when
      there's nothing left to iterate over */
  do {
    if (!lua_next(L, *(int*)map->underlying)) {
      return NULL;
    }

    get_value(map->context, -2, return_key);

    if (return_key->type == sxc_type_integer || return_key->type == sxc_type_string) {
      get_value(map->context, -1, return_value);
      return (void*)(long int)(lua_gettop(L) - 1);
    }

    lua_pop(L, 1);
  } while (1);
}


SxcMapMethods MAP_METHODS = {
  map_intget, map_intset, map_strget, map_strset, map_iter
};
