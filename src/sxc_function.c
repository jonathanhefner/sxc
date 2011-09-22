#include "sxc.h"

SxcFunction* sxc_function_new(SxcContext* context, SxcApi api) {
  if (api == NULL) return NULL;
  return (context->methods->function_wrap)(context, api);
}

void sxc_function_invoke(SxcFunction* function, SxcValue** args, int argcount, SxcValue* return_value) {
  (function->methods->invoke)(function, args, argcount, return_value);
}
