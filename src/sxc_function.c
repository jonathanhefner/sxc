#include "sxc.h"

/* TODO? remove b/c sxc_map_deftype obviates the need for this */
SxcFunction* sxc_function_new(SxcContext* context, SxcLibFunction func) {
  if (func == NULL) return NULL;
  return (context->binding->function_wrap)(context, func);
}

void sxc_function_invoke(SxcFunction* function, SxcValue** args, int argcount, SxcValue* return_value) {
  (function->binding->invoke)(function, args, argcount, return_value);
}
