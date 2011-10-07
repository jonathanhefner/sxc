#include "lua51_sxc.h"


static void map_intget(SxcMap* map, int key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;

  /* deal with 0-based vs 1-based indexing */
  if (map->is_list == TABLE_MAYBE_LIST) {
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
  /* WARNING this logic is not fault-proof.  For example, if a table is supposed
      to be a list with nil as its first and only element, and a library's first
      interaction with the table is to set its second element, the table will
      still be marked as a list, but the second element will become the first.
      This is because a first and only element of nil is no different than being
      empty and because "empty" tables can be ambiguously interpreted as either
      lists or hashmaps. */
  /* TODO? add some global constant (e.g. SXC_NIL) to the lua environment that
      is not nil, but is returned from sxc functions as nil, so that lists with
      nil first elements can effectively be created */
  if (map->is_list == TABLE_MAYBE_LIST) {
    map->is_list = table_is_list(L, *(int*)map->underlying);
    if (map->is_list == TABLE_MAYBE_LIST) {
      map->is_list = key == 0 ? TABLE_IS_LIST : TABLE_NOT_LIST;
    }
  }
  if (map->is_list) {
    key += 1;
  }

  lua_pushinteger(L, (lua_Integer)key);
  push_value(map->context, value);
  lua_settable(L, *(int*)map->underlying);
}


static void map_strget(SxcMap* map, const char* key, SxcValue* return_value) {
  lua_State* L = (lua_State*)map->context->underlying;
  luaL_checkstack(L, 1 + 2, "");
  lua_getfield(L, *(int*)map->underlying, key);
  get_value(map->context, -1, return_value);
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
  int prev_index = PTR2INT(state);

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

    if (return_key->type == sxc_int || return_key->type == sxc_string) {
      /* adjust list keys to 0-based indexing */
      if (return_key->type == sxc_int && map->is_list) {
        return_key->data.aint -= 1;
      }

      get_value(map->context, -1, return_value);

      return INT2PTR(lua_gettop(L) - 1);
    }

    lua_pop(L, 1);
  } while (1);
}


SxcMapBinding MAP_BINDING = {
  map_intget, map_intset, map_strget, map_strset, map_length, map_iter
};
