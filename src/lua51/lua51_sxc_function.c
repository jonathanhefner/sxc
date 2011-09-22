#include "lua51_sxc.h"

static void function_invoke(SxcFunction* function, SxcValue** args, const int argcount, SxcValue* return_value) {
  lua_State* L = (lua_State*)function->context->underlying;
  int i;

  luaL_checkstack(L, 1 + argcount, "");

  /* push function */
  lua_pushvalue(L, *(int*)(function->underlying));

  /* push args */
  for (i = 0; i < argcount; i += 1) {
    push_value(function->context, args[i]);
  }

  /* invoke function */
  lua_call(L, argcount, 1);

  /* get return value */
  get_value(function->context, -1, return_value);
  /* NOTE the value stays on the lua stack (i.e. is not popped) so that
      return_value's underlying stack index is valid */
}


SxcFunctionMethods FUNCTION_METHODS = {
  function_invoke
};
