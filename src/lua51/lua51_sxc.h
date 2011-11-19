#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include "../sxc.h"

#define MAPTYPE_CTORS_KEY ("sxc_maptype_ctors")
#define TABLE_IS_LIST (1)
#define TABLE_NOT_LIST (0)
#define TABLE_MAYBE_LIST (-1)

/* NOTE we assume that sizeof(void*) >= sizeof(int) */
#define INT2PTR(x) ((void*)(long int)(x))
#define PTR2INT(x) ((int)(long int)(x))


int l_libfunc_invoke(lua_State* L);
int libfunc_invoke(SxcLibFunc func, lua_State* L, const int argcount);
void get_value(int index, SxcValue* return_value);
void pop_value(SxcValue* return_value);
void push_value(SxcValue* value);


extern SxcStringBinding STRING_BINDING;
extern SxcMapBinding MAP_BINDING;
extern SxcFuncBinding FUNC_BINDING;
extern SxcContextBinding CONTEXT_BINDING;
