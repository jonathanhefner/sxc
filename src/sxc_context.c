#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
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

void sxc_value_intern(SxcValue* value);


/* TODO? change allocated chunks to be stored in a global linked list (with
    atomic, lock-free next pointers) and rarely freed, and reduce the initial
    space allocated on the stack (to zero?) */
void* sxc_context_alloc(SxcContext* context, int size) {
  SxcMemoryChunk* walker = &(context->_firstchunk);
  void* retval;
  int new_free_space;

printf("in sxc_context_alloc, size: %d, first chunk free: %d\n", size, walker->free_space);

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
      return sxc_context_error(context, "Error: out of memory");
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

/*
void* sxc_context_realloc(SxcContext* context) {
  * might need to store len before alloc'd data
  * could store 0 for data that takes up the entire chunk (i.e. chunk was alloc'd just for that data)
}

void sxc_context_free()*/


void* sxc_context_error(SxcContext* context, char* message_format, ...) {
  va_list args;
  char* buffer;
  int buffer_len;
  int actual_len;

  /* Step 1: Format error message */
  /* if compiling under the C99 standard, use vsnprintf for safety,
      otherwise, do the best we can by allocating extra buffer space */
  #if __STDC_VERSION__ >= 199901L
    buffer_len = strlen(message_format) * 8;
    buffer = sxc_context_alloc(context, buffer_len);
    actual_len = vsnprintf(buffer, buffer_len, message_format, args);

    /* proper vsnprintf implementations return length that should have been
        written (not including null terminator)... */
    if ((actual_len + 1) > buffer_len) {
      /* TODO sxc_context_free(buffer) */
      buffer_len = actual_len + 1;
      buffer = sxc_context_alloc(context, buffer_len);

      va_start(args, message_format);
      vsnprintf(buffer, buffer_len, message_format, args);
      va_end(args);
    } else {
      /* ...but some older vsnprintf implementations return -1 when buffer_len is exceeded... */
      while (actual_len < 0) {
        /* TODO sxc_context_free(buffer) */
        buffer_len *= 4;
        buffer = sxc_context_alloc(context, buffer_len);

        va_start(args, message_format);
        actual_len = vsnprintf(buffer, buffer_len, message_format, args);
        va_end(args);
      }
    }

  #else
    buffer_len = strlen(message_format) * 32;
    buffer = sxc_context_alloc(context, buffer_len);

    va_start(args, message_format);
    actual_len = vsprintf(buffer, message_format, args);
    va_end(args);
  #endif

  /* Step 2: Intern formatted error message */
  sxc_value_set_string(&context->return_value, sxc_string_new(context, buffer, actual_len, FALSE));
  context->has_error = TRUE;
  /* TODO sxc_context_free(buffer) */

  /* Step 3: Long jump back to C library's invocation point */
  LONGJMP(*(JMP_BUF*)(context->_jmpbuf), 1);

  return NULL;
}


void sxc_context_get_arg(SxcContext* context, int index, SxcValue* return_value) {
  if (return_value->context == NULL) {
    return_value->context = context;
  }

  if (index < context->argcount) {
    (context->methods->get_arg)(context, index, return_value);
  } else {
    sxc_value_set_null(return_value);
  }
}


SxcValue* sxc_context_try(SxcContext* context, SxcContextMethods* methods, void* underlying, int argcount, SxcApi api) {
  JMP_BUF jmpbuf;

  context->methods = methods;
  context->underlying = underlying;
  context->argcount = argcount;
  context->return_value = (SxcValue){context, sxc_type_null, {0}};
  context->_jmpbuf = &jmpbuf;
  context->_tmpval = (SxcValue){context, sxc_type_null, {0}};
  context->_firstchunk = (SxcMemoryChunk){SXC_MEMORY_CHUNK_INIT_SIZE, NULL, 0, 0};

  if (!(context->has_error = SETJMP(jmpbuf))) {
    (api)(context, &context->return_value);
  }

  sxc_value_intern(&context->return_value);

  return &(context->return_value);
}


void sxc_context_finally(SxcContext* context) {
  SxcMemoryChunk* walker = context->_firstchunk.next_chunk;
  while (walker != NULL) {
    context->_firstchunk.next_chunk = walker->next_chunk;
    free(walker);
    walker = context->_firstchunk.next_chunk;
  }
}
