#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include "../sxc.h"


int l_sxc_invoke(lua_State* L);


short int table_is_list(lua_State* L, int index);
SxcString* get_string(SxcContext* context, int index);
SxcMap* get_map(SxcContext* context, int index);
SxcFunction* get_function(SxcContext* context, int index);
void get_value(SxcContext* context, int index, SxcValue* return_value);
void push_value(SxcContext* context, SxcValue* value);


extern SxcMapMethods MAP_METHODS;
extern SxcFunctionMethods FUNCTION_METHODS;
extern SxcContextMethods CONTEXT_METHODS;
