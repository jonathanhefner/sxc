#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "sxc.h"

#ifndef isnan
  #define isnan(X) ((X) != (X))
#endif

#ifndef isinf
  #define isinf(X) ((X) > 0 && isnan((X) - (X)))
#endif


/* Convenience typedef for use with macros */
typedef char* string;



/***** Shared Specific Conversion Functions *****/
/* NOTE these prevent defining some to_XXXX() functions in terms of others (which could be precarious) */

static int cchars_to_string(SxcContext* context, char* cchars, int length, SxcString** string) {
  SxcValue tmp_value;
  tmp_value.context = context;
  (context->binding->to_sstring)(cchars, length, &tmp_value);

  *string = (SxcString*)sxc_alloc(context, sizeof(SxcString));
  (*string)->underlying = tmp_value.data.sstring.underlying;
  (*string)->binding = tmp_value.data.sstring.binding;
  (*string)->context = context;
  /* NOTE we retrieve the data back from the sstring to ensure we have a pointer to the interned string */
  ((*string)->binding->to_cchars)((*string)->underlying, &tmp_value);
  (*string)->data = tmp_value.data.cchars.array;
  (*string)->length = tmp_value.data.cchars.length;

  return SXC_SUCCESS;
}

static int cchars_to_sstring(SxcContext* context, char* cchars, int length, void** sstring, SxcStringBinding** sstring_binding) {
  SxcValue tmp_sstring;
  tmp_sstring.context = context;
  (context->binding->to_sstring)(cchars, length, &tmp_sstring);
  *sstring = tmp_sstring.data.sstring.underlying;
  *sstring_binding = tmp_sstring.data.sstring.binding;
  return SXC_SUCCESS;
}

static int cchars_to_cstring(SxcContext* context, char* cchars, int length, int is_null_terminated, char** cstring) {
  /* cchars data can be a binary blob instead of text, in which case a null byte
      at the end of the array is actually data rather than a sentinel; but
      consumer code processing it as a cstring wouldn't know the difference
      anyway, so we just check for a null byte at the end */
  if (is_null_terminated || cchars[length - 1] == '\0') {
    *cstring = cchars;
  } else {
    *cstring = sxc_alloc(context, length + 1);
    memcpy(*cstring, cchars, length);
    *cstring[length] = '\0';
  }
  return SXC_SUCCESS;
}

static int cchars_to_cdouble(char* cchars, int length, int is_null_terminated, double* cdouble) {
  char decimal_format[32] = "%lf";
  /* create scanf format string that won't read past end of cchars, if necessary */
  if (!is_null_terminated) {
    sprintf(decimal_format, "%%%dlf", length);
  }
  return sscanf(cchars, decimal_format, cdouble) == 1 ? SXC_SUCCESS : SXC_FAILURE;
}

static int cbool_to_cchars(bool cbool, char** cchars, int* length) {
  if (cbool) {
    *cchars = "true";
    *length = 4;
  } else {
    *cchars = "false";
    *length = 5;
  }
  return SXC_SUCCESS;
}

static int cint_to_cchars(SxcContext* context, int cint, char** cchars, int* length) {
  /* format the number
   *    NOTE: max digits in a 32-bit signed int is 10 (not including sign)
   *    NOTE: max digits in a 64-bit signed int is 19 (not including sign) */
  *cchars = sxc_alloc(context, (sizeof(int) <= 4 ? 11 : 20) + 1);
  *length = sprintf(*cchars, "%d", cint);
  return SXC_SUCCESS;
}

static int cdouble_to_cchars(SxcContext* context, double cdouble, char** cchars, int* length) {
  if (isnan(cdouble)) {
    *cchars = "nan";
    *length = 3;
  } else if (cdouble < 0 && isinf(-cdouble)) {
    *cchars = "-inf";
    *length = 4;
  } else if (isinf(cdouble)) {
    *cchars = "inf";
    *length = 3;
  } else {
    /* format the number into no more than 20 characters (max digits in a 64-bit signed int is 19) */
    *cchars = sxc_alloc(context, 20 + 1);

    /* try to format as long integer first, if there is no mantissa and the integer portion fits */
    if (cdouble == (double)(long int)cdouble && sizeof(long int) <= 8) {
      *length = sprintf(*cchars, "%ld", (long int)cdouble);
    }
    /* otherwise format as a floating point in scientific notation:
     *    |<-------20------->|
     *    +1.000000000000e+000
     *       |<---12--->|
     */
    else {
      *length = sprintf(*cchars, "%1.12e", cdouble);
    }
  }
  return SXC_SUCCESS;
}

static int sstring_to_cchars(SxcContext* context, void* sstring, SxcStringBinding* sstring_binding, char** cchars, int* length) {
  SxcValue tmp_cchars;
  tmp_cchars.context = context;
  (sstring_binding->to_cchars)(sstring, &tmp_cchars);
  *cchars = tmp_cchars.data.cchars.array;
  *length = tmp_cchars.data.cchars.length;
  return SXC_SUCCESS;
}


/***** Conversion Functions *****/

/* TODO? provide the following conversions when there's only one element in the
    source array?
    - cbools => bool
    - cints => int
    - cdoubles => double
    - cstrings => cstring
*/
/* TODO? provide the following conversions to single-element arrays?
    - bool => cbools
    - int => cints
    - double => cdoubles
    - cstring => cstrings
*/
/* TODO? test for NULL even when type isn't sxc_null */

static int to_cbool(SxcValue* value, bool* dest) {
  /* TODO also parse "true" and "false" (case sensitive? trim whitespace?) */
  char* cchars;
  int length;
  double cdouble;

  switch (value->type) {
    case sxc_cbool:
      *dest = value->data.cbool;
      return SXC_SUCCESS;

    case sxc_cint:
      *dest = (value->data.cint != 0);
      return SXC_SUCCESS;

    case sxc_cdouble:
      *dest = (value->data.cdouble != 0.0);
      return SXC_SUCCESS;

        /* DRY readability macro */
        #define FROM_DOUBLE(CONVERSION)             \
          ((CONVERSION) == SXC_SUCCESS ?            \
            (*dest = (cdouble != 0.0), SXC_SUCCESS) \
            : SXC_FAILURE)

    case sxc_string:
      return FROM_DOUBLE(cchars_to_cdouble(value->data.string->data, value->data.string->length,
              value->data.string->binding->is_null_terminated, &cdouble));

    case sxc_sstring:
      return FROM_DOUBLE(sstring_to_cchars(value->context, value->data.sstring.underlying,
            value->data.sstring.binding, &cchars, &length)
        && cchars_to_cdouble(cchars, length, value->data.sstring.binding->is_null_terminated, &cdouble));

    case sxc_cstring:
      return FROM_DOUBLE(cchars_to_cdouble(value->data.cstring, -1, true, &cdouble));

        /* macro be gone! */
        #undef FROM_DOUBLE

    default:
      return SXC_FAILURE;
  }
}


static int to_cint(SxcValue* value, int* dest) {
  char* cchars;
  int length;
  double cdouble;

  switch (value->type) {
    case sxc_cint:
      *dest = value->data.cint;
      return SXC_SUCCESS;

    case sxc_cbool:
      *dest = value->data.cbool;
      return SXC_SUCCESS;

    case sxc_cdouble:
      *dest = (int)(value->data.cdouble);
      return SXC_SUCCESS;

        /* DRY readability macro */
        #define FROM_DOUBLE(CONVERSION)             \
          ((CONVERSION) == SXC_SUCCESS ?            \
            (*dest = (int)cdouble, SXC_SUCCESS) \
            : SXC_FAILURE)

    case sxc_string:
      return FROM_DOUBLE(cchars_to_cdouble(value->data.string->data, value->data.string->length,
                          value->data.string->binding->is_null_terminated, &cdouble));

    case sxc_sstring:
      return FROM_DOUBLE(sstring_to_cchars(value->context, value->data.sstring.underlying,
            value->data.sstring.binding, &cchars, &length)
        && cchars_to_cdouble(cchars, length, value->data.sstring.binding->is_null_terminated, &cdouble));

    case sxc_cstring:
      return FROM_DOUBLE(cchars_to_cdouble(value->data.cstring, -1, true, &cdouble));

        /* macro be gone! */
        #undef FROM_DOUBLE

    default:
      return SXC_FAILURE;
  }
}


static int to_cdouble(SxcValue* value, double* dest) {
  char* cchars;
  int length;

  switch (value->type) {
    case sxc_cdouble:
      *dest = value->data.cdouble;
      return SXC_SUCCESS;

    case sxc_cbool:
      *dest = value->data.cbool ? 1.0 : 0.0;
      return SXC_SUCCESS;

    case sxc_cint:
      *dest = (double)(value->data.cint);
      return SXC_SUCCESS;

    case sxc_string:
      return cchars_to_cdouble(value->data.string->data, value->data.string->length,
                                value->data.string->binding->is_null_terminated, dest);

    case sxc_sstring:
      return sstring_to_cchars(value->context, value->data.sstring.underlying,
            value->data.sstring.binding, &cchars, &length)
        && cchars_to_cdouble(cchars, length, value->data.sstring.binding->is_null_terminated, dest);

    case sxc_cstring:
      return cchars_to_cdouble(value->data.cstring, -1, true, dest);

    default:
      return SXC_FAILURE;
  }
}


static int to_string(SxcValue* value, SxcString** dest) {
  char* cchars;
  int length;

  switch (value->type) {
    case sxc_string:
      *dest = value->data.string;
      return SXC_SUCCESS;

    case sxc_sstring:
      *dest = (SxcString*)sxc_alloc(value->context, sizeof(SxcString));
      (*dest)->underlying = value->data.sstring.underlying;
      (*dest)->binding = value->data.sstring.binding;
      (*dest)->context = value->context;
      sstring_to_cchars(value->context, (*dest)->underlying, (*dest)->binding, &((*dest)->data), &((*dest)->length));
      return SXC_SUCCESS;

    case sxc_cstring:
      return cchars_to_string(value->context, value->data.cstring,
              strlen(value->data.cstring), dest);

    case sxc_cchars:
      return cchars_to_string(value->context, value->data.cchars.array,
              value->data.cchars.length, dest);

    case sxc_cbool:
      return cbool_to_cchars(value->data.cbool, &cchars, &length)
          && cchars_to_string(value->context, cchars, length, dest);

    case sxc_cint:
      return cint_to_cchars(value->context, value->data.cint, &cchars, &length)
          && cchars_to_string(value->context, cchars, length, dest);

    case sxc_cdouble:
      return cdouble_to_cchars(value->context, value->data.cdouble, &cchars, &length)
          && cchars_to_string(value->context, cchars, length, dest);

    default:
      return SXC_FAILURE;
  }
}


static int to_sstring(SxcValue* value, void** dest, SxcStringBinding** dest_binding) {
  char* cchars;
  int length;
  char cpointer_bytes[sizeof(void*)];

  switch (value->type) {
    case sxc_sstring:
      *dest = value->data.sstring.underlying;
      *dest_binding = value->data.sstring.binding;
      return SXC_SUCCESS;

    case sxc_string:
      *dest = value->data.string->underlying;
      *dest_binding = value->data.string->binding;
      return SXC_SUCCESS;

    case sxc_cchars:
      return cchars_to_sstring(value->context, value->data.cchars.array,
              value->data.cchars.length, dest, dest_binding);

    case sxc_cstring:
      return cchars_to_sstring(value->context, value->data.cstring,
              strlen(value->data.cstring), dest, dest_binding);

    case sxc_cbool:
      return cbool_to_cchars(value->data.cbool, &cchars, &length)
          && cchars_to_sstring(value->context, cchars, length, dest, dest_binding);

    case sxc_cint:
      return cint_to_cchars(value->context, value->data.cint, &cchars, &length)
          && cchars_to_sstring(value->context, cchars, length, dest, dest_binding);

    case sxc_cdouble:
      return cdouble_to_cchars(value->context, value->data.cdouble, &cchars, &length)
          && cchars_to_sstring(value->context, cchars, length, dest, dest_binding);

    case sxc_cpointer:
      /* TODO? is this portable? */
      memcpy(cpointer_bytes, &(value->data.cpointer), sizeof(void*));
      return cchars_to_sstring(value->context, cpointer_bytes, sizeof(void*), dest, dest_binding);

    default:
      return SXC_FAILURE;
  }
}


static int to_cstring(SxcValue* value, char** dest) {
  char* cchars;
  int length;

  switch (value->type) {
    case sxc_cstring:
      *dest = value->data.cstring;
      return SXC_SUCCESS;

    case sxc_cchars:
      return cchars_to_cstring(value->context, value->data.cchars.array,
              value->data.cchars.length, false, dest);
      
    case sxc_string:
      return cchars_to_cstring(value->context, value->data.string->data,
              value->data.string->length, value->data.string->binding->is_null_terminated, dest);

    case sxc_sstring:
      return sstring_to_cchars(value->context, value->data.sstring.underlying,
              value->data.sstring.binding, &cchars, &length)
        && cchars_to_cstring(value->context, cchars, length,
              value->data.sstring.binding->is_null_terminated, dest);

    case sxc_cbool:
      /* NOTE we can assume results from cbool_to_cchars are null-terminated */
      return cbool_to_cchars(value->data.cbool, dest, &length);

    case sxc_cint:
      /* NOTE we can assume results from cint_to_cchars are null-terminated */
      return cint_to_cchars(value->context, value->data.cint, dest, &length);

    case sxc_cdouble:
      /* NOTE we can assume results from cdouble_to_cchars are null-terminated */
      return cdouble_to_cchars(value->context, value->data.cdouble, dest, &length);

    default:
      return SXC_FAILURE;
  }
}


static int to_smap(SxcValue* value, void** dest, SxcMapBinding** dest_binding) {
  SxcValue tmp_value;
  int i;

  switch (value->type) {
    case sxc_smap:
      *dest = value->data.smap.underlying;
      *dest_binding = value->data.smap.binding;
      return SXC_SUCCESS;

    case sxc_map:
      *dest = value->data.map->underlying;
      *dest_binding = value->data.map->binding;
      return SXC_SUCCESS;

        /***** straightforward but verbose (and error prone) ==> macro! *****/
        #define ARRAY2SMAP(FROM_CTYPE)                                            \
          tmp_value.context = value->context;                                     \
          /* create map */                                                        \
          (value->context->binding->map_new)(MAPTYPE_LIST, &tmp_value);           \
          *dest = tmp_value.data.smap.underlying;                                 \
          *dest_binding = tmp_value.data.smap.binding;                            \
          /* put values in map */                                                 \
          tmp_value.type = sxc_c##FROM_CTYPE;                                     \
          for (i = 0; i < value->data.c##FROM_CTYPE##s.length; i += 1) {          \
            tmp_value.data.c##FROM_CTYPE = value->data.c##FROM_CTYPE##s.array[i]; \
            ((*dest_binding)->intset)(*dest, i, &tmp_value);                      \
          }
        /********************************************************************/

    case sxc_cbools:
      ARRAY2SMAP(bool)
      return SXC_SUCCESS;

    case sxc_cints:
      ARRAY2SMAP(int)
      return SXC_SUCCESS;

    case sxc_cdoubles:
      ARRAY2SMAP(double)
      return SXC_SUCCESS;

    case sxc_cstrings:
      ARRAY2SMAP(string)
      return SXC_SUCCESS;

        /***** macro be gone! *****/
        #undef ARRAY2SMAP

    default:
      return SXC_FAILURE;
  }
}


static int to_map(SxcValue* value, SxcMap** dest) {
  void* smap;
  SxcMapBinding* smap_binding;

  switch (value->type) {
    case sxc_map:
      *dest = value->data.map;
      return SXC_SUCCESS;

    default:
      if (to_smap(value, &smap, &smap_binding)) {
        *dest = (SxcMap*)sxc_alloc(value->context, sizeof(SxcMap));
        (*dest)->underlying = smap;
        (*dest)->binding = smap_binding;
        (*dest)->context = value->context;
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }
  }
}


static int to_sfunc(SxcValue* value, void** dest, SxcFuncBinding** dest_binding) {
  SxcValue tmp_value;

  switch (value->type) {
    case sxc_sfunc:
      *dest = value->data.sfunc.underlying;
      *dest_binding = value->data.sfunc.binding;
      return SXC_SUCCESS;

    case sxc_func:
      *dest = value->data.func->underlying;
      *dest_binding = value->data.func->binding;
      return SXC_SUCCESS;

    case sxc_cfunc:
      tmp_value.context = value->context;
      (value->context->binding->to_sfunc)(value->data.cfunc, &tmp_value);
      *dest = tmp_value.data.sfunc.underlying;
      *dest_binding = tmp_value.data.sfunc.binding;
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_func(SxcValue* value, SxcFunc** dest) {
  void* sfunc;
  SxcFuncBinding* sfunc_binding;

  switch (value->type) {
    case sxc_func:
      *dest = value->data.func;
      return SXC_SUCCESS;

    default:
      if (to_sfunc(value, &sfunc, &sfunc_binding)) {
        *dest = (SxcFunc*)sxc_alloc(value->context, sizeof(SxcFunc));
        (*dest)->underlying = sfunc;
        (*dest)->binding = sfunc_binding;
        (*dest)->context = value->context;
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }
  }
}


static int to_cpointer(SxcValue* value, void** dest) {
  char* cchars;
  int length;

  switch (value->type) {
    case sxc_cpointer:
      *dest = value->data.cpointer;
      return SXC_SUCCESS;

    case sxc_string:
      if (value->data.string->length == sizeof(void*)) {
        /* TODO? is this portable? */
        memcpy(dest, value->data.string->data, sizeof(void*));
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }

    case sxc_sstring:
      sstring_to_cchars(value->context, value->data.sstring.underlying,
          value->data.sstring.binding, &cchars, &length);
      if (length == sizeof(void*)) {
        /* TODO? is this portable? */
        memcpy(dest, cchars, sizeof(void*));
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }

    /* NOTE we don't support conversion from cstring or cchars.  The only reason
        SxcString is supported is because it's the only guaranteed way to put an
        arbitrary-sized blob (even one as small as a pointer) into the scripting
        environment. */
    default:
      return SXC_FAILURE;
  }
}


static int to_cfunc(SxcValue* value, SxcLibFunc** dest) {
  switch (value->type) {
    case sxc_cfunc:
      *dest = value->data.cfunc;
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cchars(SxcValue* value, char** dest, int* dest_len) {
  SxcValue tmp_value;

  switch (value->type) {
    case sxc_cchars:
      *dest = value->data.cchars.array;
      *dest_len = value->data.cchars.length;
      return SXC_SUCCESS;

    /* TODO? should the null terminator be included in the length when present?
        - by including it we make cstring => cchars => cstring idempotent
        - BUT by including it we make looping over a cchars from a cstring
          require (i < cchars.length - 1) rather than (i < cchars.length) */

    case sxc_string:
      *dest = value->data.string->data;
      *dest_len = value->data.string->length;
      return SXC_SUCCESS;

    case sxc_sstring:
      tmp_value.context = value->context;
      (value->data.sstring.binding->to_cchars)(value->data.sstring.underlying, &tmp_value);
      *dest = tmp_value.data.cchars.array;
      *dest_len = tmp_value.data.cchars.length;
      return SXC_SUCCESS;

    case sxc_cstring:
      *dest = value->data.cstring;
      *dest_len = strlen(value->data.cstring);
      return SXC_SUCCESS;

    /* TODO? try to convert to cstring in the default case... There is ambiguity
        when converting, say, a double: does cchars mean the double stringified
        or an array of 8 bytes containing the binary value of the double? */
    default:
      return SXC_FAILURE;
  }
}



/* converting between arrays and SxcMaps is straightforward but verbose, so we
    use macros to cover all the array types */
/* NOTE these macros assume value, dest, and dest_len args, as well as tmp_value
    and i local variables */
/* NOTE an array or SxcMap filled with values that have nothing to do with the
    target primivitve type (e.g. non-numeric strings targeting doubles) will be
    naturally converted to an array filled with all zero values */

#define PRIMITIVES2PRIMITIVES(FROM_CTYPE, TO_CTYPE)                       \
  *dest_len = value->data.c##FROM_CTYPE##s.length;                        \
  *dest = sxc_alloc(value->context, sizeof(TO_CTYPE) * (*dest_len));      \
  for (i = 0; i < *dest_len; i += 1) {                                    \
    (*dest)[i] = (TO_CTYPE)value->data.c##FROM_CTYPE##s.array[i];         \
  }

#define ARRAY2ARRAY(FROM_CTYPE, TO_CTYPE)                                 \
  *dest_len = value->data.c##FROM_CTYPE##s.length;                        \
  *dest = sxc_alloc(value->context, sizeof(TO_CTYPE) * (*dest_len));      \
  tmp_value.context = value->context;                                     \
  tmp_value.type = sxc_c##FROM_CTYPE;                                     \
  for (i = 0; i < *dest_len; i += 1) {                                    \
    tmp_value.data.c##FROM_CTYPE = value->data.c##FROM_CTYPE##s.array[i]; \
    if (!to_c##TO_CTYPE(&tmp_value, &((*dest)[i]))) {                     \
      (*dest)[i] = (TO_CTYPE)0;                                           \
    }                                                                     \
  }

#define MAP2ARRAY(FROM_MAP, TO_CTYPE)                                     \
  tmp_value.data.cint = sxc_map_length((FROM_MAP));                       \
  if (tmp_value.data.cint < 0) {                                          \
    return SXC_FAILURE;                                                   \
  }                                                                       \
  *dest_len = tmp_value.data.cint;                                        \
  *dest = sxc_alloc(value->context, sizeof(TO_CTYPE) * (*dest_len));      \
  tmp_value.context = value->context;                                     \
  for (i = 0; i < *dest_len; i += 1) {                                    \
    ((FROM_MAP)->binding->intget)((FROM_MAP), i, &tmp_value);             \
    if (!to_c##TO_CTYPE(&tmp_value, &((*dest)[i]))) {                     \
      (*dest)[i] = (TO_CTYPE)0;                                           \
    }                                                                     \
  }


static int to_cbools(SxcValue* value, char** dest, int* dest_len) {
  SxcValue tmp_value;
  SxcMap tmp_map;
  int i;

  switch (value->type) {
    case sxc_cbools:
      *dest_len = value->data.cbools.length;
      *dest = value->data.cbools.array;
      return SXC_SUCCESS;

    case sxc_cints:
      PRIMITIVES2PRIMITIVES(int, bool)
      return SXC_SUCCESS;

    case sxc_cdoubles:
      PRIMITIVES2PRIMITIVES(double, bool)
      return SXC_SUCCESS;

    case sxc_cstrings:
      ARRAY2ARRAY(string, bool)
      return SXC_SUCCESS;

    case sxc_map:
      MAP2ARRAY(value->data.map, bool)
      return SXC_SUCCESS;

    case sxc_smap:
      tmp_map.underlying = value->data.smap.underlying;
      tmp_map.binding = value->data.smap.binding;
      tmp_map.context = value->context;
      MAP2ARRAY(&tmp_map, bool)
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cints(SxcValue* value, int** dest, int* dest_len) {
  SxcValue tmp_value;
  SxcMap tmp_map;
  int i;

  switch (value->type) {
    case sxc_cints:
      *dest_len = value->data.cints.length;
      *dest = value->data.cints.array;
      return SXC_SUCCESS;

    case sxc_cbools:
      PRIMITIVES2PRIMITIVES(bool, int)
      return SXC_SUCCESS;

    case sxc_cdoubles:
      PRIMITIVES2PRIMITIVES(double, int)
      return SXC_SUCCESS;

    case sxc_cstrings:
      ARRAY2ARRAY(string, int)
      return SXC_SUCCESS;

    case sxc_map:
      MAP2ARRAY(value->data.map, int)
      return SXC_SUCCESS;

    case sxc_smap:
      tmp_map.underlying = value->data.smap.underlying;
      tmp_map.binding = value->data.smap.binding;
      tmp_map.context = value->context;
      MAP2ARRAY(&tmp_map, int)
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cdoubles(SxcValue* value, double** dest, int* dest_len) {
  SxcValue tmp_value;
  SxcMap tmp_map;
  int i;

  switch (value->type) {
    case sxc_cdoubles:
      *dest_len = value->data.cdoubles.length;
      *dest = value->data.cdoubles.array;
      return SXC_SUCCESS;

    case sxc_cbools:
      PRIMITIVES2PRIMITIVES(bool, double)
      return SXC_SUCCESS;

    case sxc_cints:
      PRIMITIVES2PRIMITIVES(int, double)
      return SXC_SUCCESS;

    case sxc_cstrings:
      ARRAY2ARRAY(string, double)
      return SXC_SUCCESS;

    case sxc_map:
      MAP2ARRAY(value->data.map, double)
      return SXC_SUCCESS;

    case sxc_smap:
      tmp_map.underlying = value->data.smap.underlying;
      tmp_map.binding = value->data.smap.binding;
      tmp_map.context = value->context;
      MAP2ARRAY(&tmp_map, double)
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cstrings(SxcValue* value, char*** dest, int* dest_len) {
  SxcValue tmp_value;
  SxcMap tmp_map;
  int i;

  switch (value->type) {
    case sxc_cstrings:
      *dest_len = value->data.cstrings.length;
      *dest = value->data.cstrings.array;
      return SXC_SUCCESS;

    case sxc_cbools:
      ARRAY2ARRAY(bool, string)
      return SXC_SUCCESS;

    case sxc_cints:
      ARRAY2ARRAY(int, string)
      return SXC_SUCCESS;

    case sxc_cdoubles:
      ARRAY2ARRAY(double, string)
      return SXC_SUCCESS;

    case sxc_map:
      MAP2ARRAY(value->data.map, string)
      return SXC_SUCCESS;

    case sxc_smap:
      tmp_map.underlying = value->data.smap.underlying;
      tmp_map.binding = value->data.smap.binding;
      tmp_map.context = value->context;
      MAP2ARRAY(&tmp_map, string)
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


/***** macros be gone! *****/
#undef PRIMITIVES2PRIMITIVES
#undef ARRAY2ARRAY
#undef MAP2ARRAY



/***** SxcValue Functions *****/

int sxc_value_getv(SxcValue* value, SxcDataType type, va_list varg) {
  /* these variables are necessary because evaluation of function arguments in C
      is not well-defined (god knows why), thus the va_arg macro gets confused
      when used as a to_XXXX() arg */
  void* dest;
  void* dest_binding;
  int* dest_len;

  if (type != sxc_null) {
    dest = va_arg(varg, void*);
  }

  switch (type) {
    case sxc_cbool:
      return to_cbool(value, (bool*)dest);
    case sxc_cint:
      return to_cint(value, (int*)dest);
    case sxc_cdouble:
      return to_cdouble(value, (double*)dest);

    case sxc_string:
      return to_string(value, (SxcString**)dest);
    case sxc_map:
      return to_map(value, (SxcMap**)dest);
    case sxc_func:
      return to_func(value, (SxcFunc**)dest);

    case sxc_sstring:
      dest_binding = va_arg(varg, SxcStringBinding**);
      return to_sstring(value, (void**)dest, (SxcStringBinding**)dest_binding);
    case sxc_smap:
      dest_binding = va_arg(varg, SxcMapBinding**);
      return to_smap(value, (void**)dest, (SxcMapBinding**)dest_binding);
    case sxc_sfunc:
      dest_binding = va_arg(varg, SxcFuncBinding**);
      return to_sfunc(value, (void**)dest, (SxcFuncBinding**)dest_binding);

    case sxc_cstring:
      return to_cstring(value, (char**)dest);
    case sxc_cpointer:
      return to_cpointer(value, (void**)dest);
    case sxc_cfunc:
      return to_cfunc(value, (SxcLibFunc**)dest);
    case sxc_cchars:
      dest_len = va_arg(varg, int*);
      return to_cchars(value, (char**)dest, dest_len);
    case sxc_cbools:
      dest_len = va_arg(varg, int*);
      return to_cbools(value, (char**)dest, dest_len);
    case sxc_cints:
      dest_len = va_arg(varg, int*);
      return to_cints(value, (int**)dest, dest_len);
    case sxc_cdoubles:
      dest_len = va_arg(varg, int*);
      return to_cdoubles(value, (double**)dest, dest_len);
    case sxc_cstrings:
      dest_len = va_arg(varg, int*);
      return to_cstrings(value, (char***)dest, dest_len);

    default:
      if (type == sxc_value) {
        *(va_arg(varg, SxcValue*)) = *value;
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }
  }
}


int sxc_value_get(SxcValue* value, SxcDataType type, SXC_DATA_DEST) {
  va_list varg;
  int retval;

  va_start(varg, type);
  retval = sxc_value_getv(value, type, varg);
  va_end(varg);
  return retval;
}


void sxc_value_setv(SxcValue* value, SxcDataType type, va_list varg) {
  value->type = type;

  if (type != sxc_null) {
    switch (type) {
      case sxc_cbool:
        /* NOTE char var args are always promoted to int (see http://c-faq.com/varargs/float.html) */
        value->data.cbool = (char)va_arg(varg, int); /* TODO? coerce to 0 or 1 */
      case sxc_cint:
        value->data.cint = va_arg(varg, int);
        break;
      case sxc_cdouble:
        value->data.cdouble = va_arg(varg, double);
        break;

      case sxc_string:
      case sxc_map:
      case sxc_func:
        value->data._pointer_store = va_arg(varg, void*);
        break;

      case sxc_sstring:
      case sxc_smap:
      case sxc_sfunc:
        value->data._binding_store.underlying = va_arg(varg, void*);
        value->data._binding_store.binding = va_arg(varg, void*);
        break;

      case sxc_cstring:
      case sxc_cpointer:
      case sxc_cfunc:
        value->data._pointer_store = va_arg(varg, void*);
        break;

      case sxc_cchars:
      case sxc_cbools:
      case sxc_cints:
      case sxc_cdoubles:
      case sxc_cstrings:
        value->data._array_store.array = va_arg(varg, void*);
        value->data._array_store.length = va_arg(varg, int);
        break;

      default:
        if (type == sxc_value) {
          *value = *va_arg(varg, SxcValue*);
        } else {
          /* TODO unrecognized/invalid type error... but a longjump would
              probably prevent a closing va_end() */
        }
        break;
    }
  }
}


void sxc_value_set(SxcValue* value, SxcDataType type, SXC_DATA_ARG) {
  va_list varg;

  va_start(varg, type);
  sxc_value_setv(value, type, varg);
  va_end(varg);
}


void sxc_value_snormalize(SxcValue* value) {
  SxcData data;

  switch (value->type) {
    case sxc_string:
    case sxc_cstring:
    case sxc_cpointer:
    case sxc_cchars:
      to_sstring(value, &data.sstring.underlying, &data.sstring.binding);
      value->type = sxc_sstring;
      value->data = data;
      break;

    case sxc_map:
    case sxc_cbools:
    case sxc_cints:
    case sxc_cdoubles:
    case sxc_cstrings:
      to_smap(value, &data.smap.underlying, &data.smap.binding);
      value->type = sxc_smap;
      value->data = data;
      break;

    case sxc_func:
    case sxc_cfunc:
      to_sfunc(value, &data.sfunc.underlying, &data.sfunc.binding);
      value->type = sxc_sfunc;
      value->data = data;
      break;

    default:
      /* already interned */
      break;
  }
}

/* NOTE not the best name for this function, but it mirrors sxc_value_intern() */
void sxc_value_cnormalize(SxcValue* value) {
  SxcData data;

  switch (value->type) {
    case sxc_sstring:
      to_string(value, &(data.string));
      value->type = sxc_string;
      value->data.string = data.string;
      break;

    case sxc_smap:
      to_map(value, &(data.map));
      value->type = sxc_map;
      value->data.map = data.map;
      break;

    case sxc_sfunc:
      to_func(value, &(data.func));
      value->type = sxc_func;
      value->data.func = data.func;
      break;

    default:
      /* already "externed" */
      break;
  }
}
