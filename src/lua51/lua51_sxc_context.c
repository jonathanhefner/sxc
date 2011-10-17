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


static int l_maptype_metatable_index(lua_State* L) {
  SxcLibFunc* getter;

  /* check for method first (return if found) */
  lua_pushvalue(L, 2/*property name*/);
  lua_rawget(L, lua_upvalueindex(1/*methods*/));

  if (!lua_isnil(L, -1)) {
    /* method is on top of stack */
    return 1;
  } else {
    lua_pop(L, 1);

    /* check getters next (invoke if found) */
    lua_pushvalue(L, 2/*property name*/);
    lua_rawget(L, lua_upvalueindex(2/*getters*/));
    getter = (SxcLibFunc*)lua_touserdata(L, -1);

    if (getter != NULL) {
      libfunc_invoke(getter, L, 1);
      /* keep getter return value */
      lua_settop(L, 3);
      return 1;
    } else {
      /* nil is on top of stack */
      return 1;
    }
  }
}

static int l_maptype_metatable_newindex(lua_State* L) {
  SxcLibFunc* setter;

  /* check for setter */
  lua_pushvalue(L, 2/*property name*/); /* 2nd arg is property name */
  lua_rawget(L, lua_upvalueindex(1));
  setter = (SxcLibFunc*)lua_touserdata(L, -1);
  lua_pop(L, 1);

  if (setter != NULL) {
    /* manipulate args on stack for setter */
    lua_replace(L, 2/*property name*/); /* make property value (3rd) arg the 2nd arg */

    libfunc_invoke(setter, L, 2);

    /* ignore setter return value */
    lua_settop(L, 3);
  } else {
    /* otherwise simply set the value */
    lua_rawset(L, 1/*object*/);
    /* push nil to replace the popped property name and value args */
    lua_pushnil(L);
    lua_pushnil(L);
  }

  return 0;
}

static int l_maptype_metatable_call(lua_State* L) {
  SxcLibFunc* initializer = (SxcLibFunc*)lua_touserdata(L, lua_upvalueindex(2));

  /* construct object */
  lua_newtable(L);

  /* set metatable */
  lua_pushvalue(L, lua_upvalueindex(1/*metatable*/));
  lua_setmetatable(L, -2/*object*/);

  /* invoke initializer */
  if (initializer != NULL) {
    /* manipulate args on stack for initializer */
    lua_insert(L, 1); /* move object to 1st arg position */

    libfunc_invoke(initializer, L, lua_gettop(L));

    /* ignore initializer return value and put object in place to be returned */
    lua_pop(L, 2);
    lua_pushvalue(L, 1/*object*/);
  }

  return 1;
}


static void maptype_metatable(lua_State* L, int is_static, const SxcLibMethod* methods, const SxcLibProperty* properties) {
  int i;
  luaL_checkstack(L, 6, "");

printf("in maptype_metatable, is_static:%d\n", is_static);

  lua_newtable(L);

  /* create metatable.__index() as a C closure */
  lua_pushliteral(L, "__index");
    /* 1st up-value is methods */
    lua_newtable(L);
    for (i = 0; methods[i].name != NULL; i += 1) {
      if (is_static == methods[i].is_static || (is_static && methods[i].is_static)) {
        lua_pushstring(L, methods[i].name);
          lua_pushlightuserdata(L, methods[i].func);
        lua_pushcclosure(L, l_libfunc_invoke, 1);
        lua_rawset(L, -3);
      }
    }
    /* 2nd up-value is getters */
    lua_newtable(L);
    for (i = 0; properties[i].name != NULL; i += 1) {
      if (properties[i].getter != NULL
          && (is_static == methods[i].is_static || (is_static && methods[i].is_static))) {
        lua_pushstring(L, properties[i].name);
        lua_pushlightuserdata(L, properties[i].getter);
        lua_rawset(L, -3);
      }
    }
  lua_pushcclosure(L, l_maptype_metatable_index, 2);
  lua_rawset (L, -3);

  /* TODO when there is a setter but no getter, create a getter that returns the
      last set value */

  /* create metatable.__newindex() as a C closure */
  lua_pushliteral(L, "__newindex");
    /* 1st and only up-value is setters */
    lua_newtable(L);
    for (i = 0; properties[i].name != NULL; i += 1) {
      if (properties[i].setter != NULL
          && (is_static == methods[i].is_static || (is_static && methods[i].is_static))) {
        lua_pushstring(L, properties[i].name);
        lua_pushlightuserdata(L, properties[i].setter);
        lua_rawset(L, -3);
      }
    }
  lua_pushcclosure(L, l_maptype_metatable_newindex, 1);
  lua_rawset (L, -3);

  /* metatable is now on top of stack */
printf("done maptype_metatable\n");
}


static void* map_newtype(SxcContext* context, const char* name, SxcLibFunc initializer,
                          const SxcLibMethod* methods, const SxcLibProperty* properties) {
  lua_State* L = (lua_State*)context->underlying;
  int ctor_index;

printf("in map_newtype\n");

  luaL_checkstack(L, 8, "");

  /* prepare to add entry to registry of map type ctors */
  lua_getfield(L, LUA_REGISTRYINDEX, MAPTYPE_CTORS_KEY);
  lua_pushnil(L); /* will be replaced by __call closure */
  ctor_index = lua_gettop(L);

  /* create "class"/ctor global */
  lua_newtable(L);
    maptype_metatable(L, 1, methods, properties); /* pushes metatable */
      /* add __call metamethod */
      lua_pushliteral(L, "__call");
        /* 1st up-value for __call is instance metatable */
        maptype_metatable(L, 0, methods, properties);
        /* 2nd up-value for __call is initializer */
        lua_pushlightuserdata(L, initializer);
      lua_pushcclosure(L, l_maptype_metatable_call, 2);
        /* save a copy of the closure */
        lua_pushvalue(L, -1);
        lua_replace(L, ctor_index);
      lua_rawset (L, -3);
    lua_setmetatable(L, -2); /* pops metatable */
  lua_setglobal(L, name);

printf("done class/ctor creation\n");

  /* finally store map type ctor */
  ctor_index = lua_objlen(L, -2) + 1;
printf("class/ctor index: %d\n", ctor_index);
  lua_rawseti(L, -2, ctor_index);
printf("class/ctor stored\n");
  lua_pop(L, 1);

printf("done map_newtype\n");

  return INT2PTR(ctor_index);
}


static SxcMap* map_new(SxcContext* context, void* map_type) {
  lua_State* L = (lua_State*)context->underlying;

  luaL_checkstack(L, 4 + 2, "");

  if (map_type == MAPTYPE_HASH || map_type == MAPTYPE_LIST) {
    lua_newtable(L);
    return get_map(context, -1);
  } else {
    /* TODO actually invoke the ctor with some given args, instead of simply
        returning an un-initialized table with associated metatable */
    lua_newtable(L);

    /* associate metatable */
    lua_getfield(L, LUA_REGISTRYINDEX, MAPTYPE_CTORS_KEY); /* put ctor store on stack */
    lua_rawgeti(L, -1, PTR2INT(map_type)); /* put ctor on stack */
    lua_getupvalue(L, -1, 1); /* put metatable on stack */
    lua_setmetatable(L, -4);
    lua_pop(L, 2);

    return get_map(context, -1);
  }
}


static SxcFunc* function_wrap(SxcContext* context, SxcLibFunc func) {
  lua_State* L = (lua_State*)context->underlying;

  luaL_checkstack(L, 2 + 2, "");
  lua_pushlightuserdata(L, func);
  lua_pushcclosure(L, l_libfunc_invoke, 1);
  return get_func(context, -1);
}


SxcContextBinding CONTEXT_BINDING = {
  context_get_arg, string_intern, map_newtype, map_new, function_wrap
};
