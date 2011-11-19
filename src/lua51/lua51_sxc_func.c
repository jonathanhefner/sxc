#include "lua51_sxc.h"


#include <stdio.h>
static void func_invoke(void* underlying, SxcValue** args, int argcount, SxcValue* return_value) {
  lua_State* L = (lua_State*)(return_value->context->underlying);
  int i;

printf("in lua function_invoke\n");

  luaL_checkstack(L, 1 + argcount, "");

  /* push function */
  lua_pushvalue(L, PTR2INT(underlying));
printf("done pushing function\n");

  /* push args */
  for (i = 0; i < argcount; i += 1) {
    push_value(args[i]);
printf("done pushing arg %d\n", i);
  }

  /* invoke function */
  lua_call(L, argcount, 1);
printf("done calling func\n");

  /* get return value */
  pop_value(return_value);
printf("done get_value\n");
}


SxcFuncBinding FUNC_BINDING = {
  func_invoke
};
