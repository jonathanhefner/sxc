#include <stdarg.h>
#include "sxc.h"

int sxc_value_getv(SxcValue* value, SxcDataType type, va_list varg);
void sxc_value_setv(SxcValue* value, SxcDataType type, va_list varg);


/* TODO? remove b/c sxc_map_deftype obviates the need for this */
SxcFunc* sxc_func_new(SxcContext* context, SxcLibFunc func) {
  if (func == NULL) return NULL;
  return (context->binding->func_wrap)(context, func);
}

#include <stdio.h>
/* TODO this function signature still feels off... how can it be more intuitive? */
int sxc_func_invoke(SxcFunc* function, int argcount, SxcDataType return_type, SXC_DATA_DEST_ARGS) {
  va_list varg;

  const int default_argcount = 32;
  SxcValue values[default_argcount];
  SxcValue* arg_values = values;
  SxcValue* valueptrs[default_argcount];
  SxcValue** arg_valueptrs = valueptrs;
  SxcValue return_value;

  int i;
  SxcDataType type;
  int actual_return;

printf("in sxc_function_invoke\n");

  /* allocate more room for args if necessary (unlikely) */
  if (argcount > default_argcount) {
    arg_valueptrs = sxc_alloc(function->context, argcount * (sizeof(SxcValue*) + sizeof(SxcValue)));
    arg_values = (SxcValue*)(arg_valueptrs + argcount);
  }

  /* skip over dest part of varargs (we come back to it later) */
  return_value.context = function->context;
  return_value.type = sxc_null;
  va_start(varg, return_type);
  sxc_value_getv(&return_value, return_type, varg);
printf("done skipping dest\n");

  /* point arg_valueptrs to arg_values, and put rest of varargs into them */
  for (i = 0; i < argcount; i += 1) {
    arg_valueptrs[i] = &arg_values[i];

    arg_valueptrs[i]->context = function->context;
    type = va_arg(varg, SxcDataType);
    sxc_value_setv(arg_valueptrs[i], type, varg);
printf("done setting arg %d\n", i);
    sxc_value_intern(arg_valueptrs[i]);
printf("done interning arg %d\n", i);
  }
  va_end(varg);

  /* invoke function */
  (function->binding->invoke)(function, arg_valueptrs, argcount, &return_value);
printf("done invoking func\n");

  /* extract return_value to dest */
  va_start(varg, return_type);
  actual_return = sxc_value_getv(&return_value, return_type, varg);
printf("done extracting return_value\n");
  va_end(varg);

  return actual_return;
}
