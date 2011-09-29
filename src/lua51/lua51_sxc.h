#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include "../sxc.h"

#define MAPTYPE_CTORS_KEY ("sxc_maptype_ctors")
#define TABLE_IS_LIST (1)
#define TABLE_NOT_LIST (0)
#define TABLE_MAYBE_LIST (-1)

#define INT2PTR(x) ((void*)(long int)x)
#define PTR2INT(x) ((int)(long int)x)


int l_libfunction_invoke(lua_State* L);

int libfunction_invoke(SxcLibFunction func, lua_State* L, const int argcount);
short int table_is_list(lua_State* L, int index);
SxcString* get_string(SxcContext* context, int index);
SxcMap* get_map(SxcContext* context, int index, int is_list);
SxcFunction* get_function(SxcContext* context, int index);
void get_value(SxcContext* context, int index, SxcValue* return_value);
void push_value(SxcContext* context, SxcValue* value);


extern SxcMapBinding MAP_BINDING;
extern SxcFunctionBinding FUNCTION_BINDING;
extern SxcContextBinding CONTEXT_BINDING;
