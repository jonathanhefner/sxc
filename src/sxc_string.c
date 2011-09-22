#include <string.h>
#include "sxc.h"


/* string methods
 * - new
 * - substr
 * - tobuffer
 *
 * buffer methods
 * - new
 * - ensure
 * - append
 * - tostring
*/

#include <stdio.h>
SxcString* sxc_string_new(SxcContext* context, char* data, int length, int lazy) {
  SxcString* s;
printf("in sxc_string_new\n");
  if (data == NULL) return NULL;

printf("...data != NULL\n");
printf("...context == NULL: %d\n", context == NULL);

  if (length < 0) {
printf("...computing strlen\n");
    length = strlen(data);
  }

  if (lazy) {
printf("...creating lazy string\n");
    s = (SxcString*)sxc_context_alloc(context, sizeof(SxcString));
printf("...s == NULL: %d\n", s == NULL);
printf("...lazy string context\n");
    s->context = context;
printf("...lazy string underlying\n");
    s->underlying = NULL;
printf("...lazy string data\n");
    s->data = data;
printf("...lazy string length\n");
    s->length = length;
printf("...done lazy creation\n");
  } else {
printf("...interning string\n");
    s = (context->methods->string_intern)(context, data, length);
  }

printf("...done sxc_string_new\n");

  return s;
}
