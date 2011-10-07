#include "lua51_sxc.h"

#include <stdio.h>
static void func_invoke(SxcFunc* func, SxcValue** args, const int argcount, SxcValue* return_value) {
  lua_State* L = (lua_State*)func->context->underlying;
  int i;

printf("in lua function_invoke\n");

  luaL_checkstack(L, 1 + argcount, "");

  /* push function */
  lua_pushvalue(L, *(int*)(func->underlying));
printf("done pushing function\n");

  /* push args */
  for (i = 0; i < argcount; i += 1) {
    push_value(func->context, args[i]);
printf("done pushing arg %d\n", i);
  }

  /* invoke function */
  lua_call(L, argcount, 1);
printf("done calling func\n");

  /* get return value */
  get_value(func->context, -1, return_value);
  /* NOTE the value stays on the lua stack (i.e. is not popped) so that
      return_value's underlying stack index is valid */
printf("done get_value\n");
}


SxcFuncBinding FUNC_BINDING = {
  func_invoke
};