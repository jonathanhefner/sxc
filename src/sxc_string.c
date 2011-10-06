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

/* TODO remove sxc_string_new? how necessary is it, given the varargs type system? */
SxcString* sxc_string_new(SxcContext* context, char* data, int length) {
  if (data == NULL) return NULL;

  if (length < 0) {
    length = strlen(data);
  }

  return (context->binding->string_intern)(context, data, length);
}
