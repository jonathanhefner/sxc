#include <stdarg.h>
#include "sxc.h"

void sxc_typeerror(SxcContext* context, char* value_name, SxcDataType expected_type, SxcValue* actual_value);
int sxc_value_getv(SxcValue* value, SxcDataType type, va_list varg);
void sxc_value_setv(SxcValue* value, SxcDataType type, va_list varg);
void sxc_value_snormalize(SxcValue* value);
void sxc_value_cnormalize(SxcValue* value);


#include <stdio.h>
/* TODO this function signature still feels off... how can it be more intuitive? */
void sxc_func_invoke(SxcFunc* function, int argcount, SxcDataType return_type, SXC_DATA_DEST_ARGS) {
  va_list varg;

  const int default_argcount = 32;
  SxcValue values[default_argcount];
  SxcValue* arg_values = values;
  SxcValue* valueptrs[default_argcount];
  SxcValue** arg_valueptrs = valueptrs;
  SxcValue return_value;

  int i;
  SxcDataType type;
  int has_return_value;

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
    sxc_value_snormalize(arg_valueptrs[i]);
printf("done interning arg %d\n", i);
  }
  va_end(varg);

  /* invoke function */
  (function->binding->invoke)(function->underlying, arg_valueptrs, argcount, &return_value);
printf("done invoking func\n");

  /* extract return_value to dest */
  if (return_type != sxc_null) {
    if (return_type == sxc_value) {
      sxc_value_cnormalize(&return_value);
    }

    va_start(varg, return_type);
    has_return_value = sxc_value_getv(&return_value, return_type, varg);
printf("done extracting return_value\n");
    va_end(varg);

    if (has_return_value != SXC_SUCCESS) {
      sxc_typeerror(function->context, "return value", return_type, &return_value);
    }
  }
}
