#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "sxc.h"

/* Try to use sigsetjmp instead of setjmp if possible--certain implementations
    of setjmp are very slow because they save signal handling data.
    See http://tratt.net/laurie/tech_articles/articles/timing_setjmp_and_the_joy_of_standards */
#include <setjmp.h>
#if _POSIX_C_SOURCE >= 1 || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE)
  #define JMP_BUF sigjmp_buf
  #define SETJMP(env) (sigsetjmp(env, 0))
  #define LONGJMP(env, val) (siglongjmp(env, val))
#else
  #define JMP_BUF jmp_buf
  #define SETJMP(env) (setjmp(env))
  #define LONGJMP(env, val) (longjmp(env, val))
#endif


void sxc_typeerror(SxcContext* context, char* value_name, SxcDataType expected_type, SxcValue* actual_value);
int sxc_value_getv(SxcValue* value, SxcDataType type, va_list varg);
void sxc_value_setv(SxcValue* value, SxcDataType type, va_list varg);
void sxc_value_intern(SxcValue* value);



/* TODO? change allocated chunks to be stored in a global linked list (with
    atomic, lock-free next pointers) and rarely freed, and reduce the initial
    space allocated on the stack (to zero?) */
void* sxc_alloc(SxcContext* context, int size) {
  SxcMemoryChunk* walker = &(context->_firstchunk);
  void* retval;
  int new_free_space;

printf("in sxc_context_alloc, size: %d, first chunk free: %d\n", size, walker->free_space);

  /* TODO? should size <= 0 raise an error? (if so sxc_value.c must be fixed) */
  if (size <= 0) return NULL;

  /* find appropriate memory chunk */
  while (walker->free_space < size && walker->next_chunk != NULL) {
    walker = walker->next_chunk;
  }

  /* add new chunk to list if necessary */
  if (walker->free_space < size) {
printf("...allocating new node\n");

    /* double prev node's free space up to a max */
    new_free_space = (walker->offset + walker->free_space);
    do {
      new_free_space *= 2;
    } while (new_free_space <= size && new_free_space < SXC_MEMORY_CHUNK_MAX_SIZE);

    /* but alloc at least as much as requested */
    new_free_space = size > new_free_space ? size : new_free_space;

printf("...new node space: %d\n", new_free_space);

    walker->next_chunk = malloc(sizeof(SxcMemoryChunk) + new_free_space - 1 /* one byte already in struct */);
    if (walker->next_chunk == NULL) {
      return sxc_error(context, "Error: out of memory");
    }

    walker = walker->next_chunk;
    walker->free_space = new_free_space;
    walker->offset = 0;
    walker->next_chunk = NULL;
  }

  retval = &(walker->data) + walker->offset;
  walker->free_space -= size;
  walker->offset += size;

printf("...done sxc_context_alloc, free space: %d\n", walker->free_space);

  return retval;
}

/* TODO
void* sxc_context_realloc(SxcContext* context)
  * might need to store len before alloc'd data
  * could store 0 for data that takes up the entire chunk (i.e. chunk was alloc'd just for that data)

void sxc_context_free()
*/


void* sxc_error(SxcContext* context, const char* message_format, ...) {
  va_list varg;
  char* buffer;
  int buffer_len;
  int actual_len;

  /* Step 1: Format error message */
  /* if compiling under the C99 standard, use vsnprintf for safety,
      otherwise, do the best we can by allocating extra buffer space */
  #if __STDC_VERSION__ >= 199901L
    buffer_len = strlen(message_format) * 8;
    buffer = sxc_alloc(context, buffer_len);
    va_start(varg, message_format);
    vsnprintf(buffer, buffer_len, message_format, varg);
    va_end(varg);

    /* proper vsnprintf implementations return length that should have been
        written (not including null terminator)... */
    if ((actual_len + 1) > buffer_len) {
      /* TODO sxc_context_free(buffer) */
      buffer_len = actual_len + 1;
      buffer = sxc_alloc(context, buffer_len);

      va_start(varg, message_format);
      vsnprintf(buffer, buffer_len, message_format, varg);
      va_end(varg);
    } else {
      /* ...but some older vsnprintf implementations return -1 when buffer_len is exceeded... */
      while (actual_len < 0) {
        /* TODO sxc_context_free(buffer) */
        buffer_len *= 4;
        buffer = sxc_alloc(context, buffer_len);

        va_start(varg, message_format);
        actual_len = vsnprintf(buffer, buffer_len, message_format, varg);
        va_end(varg);
      }
    }

  #else
    buffer_len = strlen(message_format) * 32;
    buffer = sxc_alloc(context, buffer_len);

    va_start(varg, message_format);
    actual_len = vsprintf(buffer, message_format, varg);
    va_end(varg);
  #endif

  /* Step 2: Save formatted error message */
  sxc_value_set(&context->return_value, sxc_cchars, buffer, actual_len);
  context->has_error = true;

  /* Step 3: Long jump back to C library's invocation point */
  LONGJMP(*(JMP_BUF*)(context->_jmpbuf), 1);

  return NULL;
}


void sxc_typeerror(SxcContext* context, char* value_name, SxcDataType expected_type, SxcValue* actual_value) {
  /* NOTE we assume the primary use case is to extract values from the scripting
      environment, so expected types are written to inform scripters.  On the
      other hand, if the actual type is a C type (i.e. sxc_cXXXX) it isn't
      coming from the scripting environment, so we tell the programmer exactly
      what he's dealing with. */
  const char* expected_types[] = {
      /* sxc_null */      "null",
      /* sxc_bool */      "a boolean",
      /* sxc_int */       "an int",
      /* sxc_double */    "a double",
      /* sxc_string */    "a string",
      /* sxc_map */       "a map",
      /* sxc_func */      "a function",
      /* sxc_cstring */   "a string",
      /* sxc_cpointer */  "a reference to a C struct",
      /* sxc_cfunc */     "a C function",
      /* sxc_cchars */    "a string",
      /* sxc_cbools */    "a list of booleans",
      /* sxc_cints */     "a list of ints",
      /* sxc_cdoubles */  "a list of doubles",
      /* sxc_cstrings */  "a list of strings"
    };
  const char* actual_types[] = {
      /* sxc_null */      "null",
      /* sxc_bool */      "a boolean",
      /* sxc_int */       "an int",
      /* sxc_double */    "a double",
      /* sxc_string */    "a string",
      /* sxc_map */       "a map",
      /* sxc_func */      "a function",
      /* sxc_cstring */   "a C string",
      /* sxc_cpointer */  "a C pointer",
      /* sxc_cfunc */     "a C function",
      /* sxc_cchars */    "an array of chars",
      /* sxc_cbools */    "an array of booleans",
      /* sxc_cints */     "an array of ints",
      /* sxc_cdoubles */  "an array of doubles",
      /* sxc_cstrings */  "an array of strings"
    };

  if (expected_type == sxc_null) {
    sxc_error(context, "Can not extract %s because destination type "
        "is sxc_null.", value_name);
  }

  else if (expected_type >= (sizeof(expected_types) / sizeof(expected_types[0]))) {
    sxc_error(context, "Can not extract %s because destination type "
        "(%d) is unrecognized.", value_name, expected_type);
  }

  else {
    sxc_error(context, "Expected %s to be %s (or compatible).  Actual value was %s.",
        value_name,
        expected_types[expected_type],
        actual_value->type >= (sizeof(actual_types) / sizeof(actual_types[0]))
          ? "of unknown type"
          : actual_types[actual_value->type]);
    /* TODO turn value to string (if possible) and truncate (with ellipsis) if
        string is too long */
  }
}


int sxc_arg(SxcContext* context, int index, bool is_required, SxcDataType type, SXC_DATA_DEST) {
  va_list varg;
  int retval = SXC_FAILURE;
  SxcValue value;

  const char* value_name_format = "argument %d";
  char* value_name;

  if (index < context->argcount) {
    value.context = context;
    (context->binding->get_arg)(context, index, &value);

    va_start(varg, type);
    retval = sxc_value_getv(&value, type, varg);
    va_end(varg);
  }

  if (is_required && retval != SXC_SUCCESS) {
    value_name = sxc_alloc(context, sizeof(char) * (strlen(value_name_format) + 20 + 1));
    sprintf(value_name, value_name_format, index + 1 /* start counting at 1 */);

    sxc_typeerror(context, value_name, type, &value);
  }
  return retval;
}


void sxc_return(SxcContext* context, SxcDataType type, SXC_DATA_ARG) {
  va_list varg;

  va_start(varg, type);
  sxc_value_setv(&context->return_value, type, varg);
  va_end(varg);
}


void sxc_try(SxcContext* context, SxcContextBinding* binding, void* underlying, int argcount, SxcLibFunc func) {
  JMP_BUF jmpbuf;

  context->binding = binding;
  context->underlying = underlying;
  context->argcount = argcount;
  context->return_value = (SxcValue){context, sxc_null, {0}};
  context->_jmpbuf = &jmpbuf;
  context->_firstchunk = (SxcMemoryChunk){SXC_MEMORY_CHUNK_INIT_SIZE, NULL, 0, 0};

  if (!(context->has_error = SETJMP(jmpbuf))) {
    (func)(context);
  }

  sxc_value_intern(&context->return_value);
}


void sxc_finally(SxcContext* context) {
  SxcMemoryChunk* walker = context->_firstchunk.next_chunk;
  while (walker != NULL) {
    context->_firstchunk.next_chunk = walker->next_chunk;
    free(walker);
    walker = context->_firstchunk.next_chunk;
  }
}
