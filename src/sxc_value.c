#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "sxc.h"



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


static int to_double(SxcValue* value, double* dest) {
  char decimal_format[32] = "%lf";

  switch (value->type) {
    case sxc_double:
      *dest = value->data.adouble;
      return SXC_SUCCESS;

    case sxc_bool:
      *dest = value->data.abool ? 1.0 : 0.0;
      return SXC_SUCCESS;

    case sxc_int:
      *dest = (double)(value->data.aint);
      return SXC_SUCCESS;

    case sxc_string:
      /* if string isn't null-terminated, we create a scanf format string that
          won't read past end of string */
      if (!value->data.string->is_terminated) {
        sprintf(decimal_format, "%%%dlf", value->data.string->length);
      }
      return sscanf(value->data.string->data, decimal_format, dest) ?
          SXC_SUCCESS : SXC_FAILURE;

    case sxc_cstring:
      return sscanf(value->data.cstring, decimal_format, dest) ?
          SXC_SUCCESS : SXC_FAILURE;

    default:
      return SXC_FAILURE;
  }
}


static int to_bool(SxcValue* value, char* dest) {
  double parsed_string;

  switch (value->type) {
    case sxc_bool:
      *dest = value->data.abool;
      return SXC_SUCCESS;

    case sxc_int:
      *dest = (value->data.aint != 0);
      return SXC_SUCCESS;

    case sxc_double:
      *dest = (value->data.adouble != 0.0);
      return SXC_SUCCESS;

    case sxc_string:
    case sxc_cstring:
      /* TODO also parse "true" and "false" (case sensitive? trim whitespace?) */
      if (to_double(value, &parsed_string) == SXC_SUCCESS) {
        *dest = (parsed_string != 0.0);
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }

    default:
      return SXC_FAILURE;
  }
}


static int to_int(SxcValue* value, int* dest) {
  double parsed_string;

  switch (value->type) {
    case sxc_int:
      *dest = value->data.aint;
      return SXC_SUCCESS;

    case sxc_bool:
      *dest = value->data.abool;
      return SXC_SUCCESS;

    case sxc_double:
      *dest = (int)(value->data.adouble);
      return SXC_SUCCESS;

    case sxc_string:
    case sxc_cstring:
      if (to_double(value, &parsed_string) == SXC_SUCCESS) {
        *dest = (int)parsed_string;
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }

    default:
      return SXC_FAILURE;
  }
}


static int to_cstring(SxcValue* value, char** dest) {
  switch (value->type) {
    case sxc_cstring:
      *dest = value->data.cstring;
      return SXC_SUCCESS;

    case sxc_bool:
      *dest = value->data.abool ? "1" : "0";
      return SXC_SUCCESS;

    case sxc_int:
      /* format the number
       *    NOTE: max digits in a 32-bit signed int is 10 (not including sign)
       *    NOTE: max digits in a 64-bit signed int is 19 (not including sign) */
      *dest = sxc_context_alloc(value->context, (sizeof(int) <= 4 ? 11 : 20) + 1);
      sprintf(*dest, "%d", value->data.aint);
      return SXC_SUCCESS;

    case sxc_double:
      /* TODO? return literal strings for NaN, +Inf and -Inf */
      /* format the number into no more than 20 characters (max digits in a 64-bit signed int is 19) */
      *dest = sxc_context_alloc(value->context, 20 + 1);

      /* try to format as long integer first, if there is no mantissa and the integer portion fits */
      if (value->data.adouble == (double)(long int)(value->data.adouble) && sizeof(long int) <= 8) {
        sprintf(*dest, "%ld", (long int)(value->data.adouble));
      }
      /* otherwise format as a floating point in scientific notation:
       *    |<-------20------->|
       *    +1.000000000000e+000
       *       |<---12--->|
       */
      else {
        sprintf(*dest, "%1.12e", value->data.adouble);
      }
      return SXC_SUCCESS;

    case sxc_string:
      if (value->data.string->is_terminated) {
        *dest = value->data.string->data;
      } else {
        *dest = sxc_context_alloc(value->context, value->data.string->length + 1);
        memcpy(*dest, value->data.string->data, value->data.string->length);
        *dest[value->data.string->length] = '\0';
      }
      return SXC_SUCCESS;

    case sxc_cchars:
      /* NOTE cchars data can be a binary blob instead of text, in which case a
          null byte at the end of the array is actually data rather than a
          sentinel; but consumer code processing it as a cstring wouldn't know
          the difference anyway, so we just check for a null byte at the end */
      if (value->data.cchars.array[value->data.cchars.length - 1] == '\0') {
        *dest = value->data.cchars.array;
      } else {
        *dest = sxc_context_alloc(value->context, value->data.cchars.length + 1);
        memcpy(*dest, value->data.cchars.array, value->data.cchars.length);
        *dest[value->data.cchars.length] = '\0';
      }
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_string(SxcValue* value, SxcString** dest) {
  char cpointer[sizeof(void*)];
  char* cstring;

  switch (value->type) {
    case sxc_string:
      *dest = value->data.string;
      return SXC_SUCCESS;

    case sxc_cpointer:
      /* TODO? is this portable? */
      memcpy(cpointer, &(value->data.cpointer), sizeof(void*));
      *dest = sxc_string_new(value->context, cpointer, sizeof(cpointer));
      return SXC_SUCCESS;

    case sxc_cchars:
      *dest = sxc_string_new(value->context, value->data.cchars.array, value->data.cchars.length);
      return SXC_SUCCESS;

    default:
      if (to_cstring(value, &cstring)) {
        *dest = sxc_string_new(value->context, cstring, -1);
        return SXC_SUCCESS;
      } else {
        return SXC_FAILURE;
      }
  }
}


static int to_map(SxcValue* value, SxcMap** dest) {
  SxcValue element;
  int i;

  switch (value->type) {
    case sxc_map:
      *dest = value->data.map;
      return SXC_SUCCESS;

      /* copying arrays into an SxcMap is a straightforward but verbose
          operation, so we use a macro to cover all our array types */
      #define ARRAY2MAP(ARRAY_NAME, ELEMENT_TYPE, ELEMENT_DATA_NAME)        \
        *dest = sxc_map_new(value->context, MAPTYPE_LIST);                  \
        element.context = value->context;                                   \
        element.type = ELEMENT_TYPE;                                        \
        for (i = 0; i < value->data.ARRAY_NAME.length; i += 1) {            \
          element.data.ELEMENT_DATA_NAME = value->data.ARRAY_NAME.array[i]; \
          ((*dest)->binding->intset)(*dest, i, &element);                     \
        }

    case sxc_cbools:
      ARRAY2MAP(cbools, sxc_bool, abool)
      return SXC_SUCCESS;

    case sxc_cints:
      ARRAY2MAP(cints, sxc_int, aint)
      return SXC_SUCCESS;

    case sxc_cdoubles:
      ARRAY2MAP(cdoubles, sxc_double, adouble)
      return SXC_SUCCESS;

    case sxc_cstrings:
      ARRAY2MAP(cstrings, sxc_cstring, cstring)
      return SXC_SUCCESS;

      /* clean up */
      #undef ARRAY2MAP

    default:
      return SXC_FAILURE;
  }
}


static int to_function(SxcValue* value, SxcFunction** dest) {
  switch (value->type) {
    case sxc_function:
      *dest = value->data.function;
      return SXC_SUCCESS;

    case sxc_cfunction:
      *dest = sxc_function_new(value->context, value->data.cfunction);
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cpointer(SxcValue* value, void** dest) {
  switch (value->type) {
    case sxc_cpointer:
      *dest = value->data.cpointer;
      return SXC_SUCCESS;

    case sxc_string:
      /* TODO? is this portable? */
      memcpy(dest, value->data.string->data, sizeof(void*));
      return SXC_SUCCESS;

    /* NOTE we don't support conversion from cstring or cchars.  The only reason
        SxcString is supported is because it's the only guaranteed way to put an
        arbitrary-sized blob (even one as small as a pointer) into the scripting
        environment. */
    default:
      return SXC_FAILURE;
  }
}


static int to_cfunction(SxcValue* value, SxcLibFunction** dest) {
  switch (value->type) {
    case sxc_cfunction:
      *dest = value->data.cfunction;
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cchars(SxcValue* value, char** dest, int* dest_len) {
  switch (value->type) {
    case sxc_cchars:
      *dest = value->data.cchars.array;
      *dest_len = value->data.cchars.length;
      return SXC_SUCCESS;

    /* TODO? should the null terminator be included when it is present?
        - by including it we make cstring => cchars => cstring idempotent
        - BUT by including it we make looping over a cchars from a cstring
          require (i < cchars.length - 1) rather than (i < cchars.length) */

    case sxc_string:
      *dest = value->data.string->data;
      *dest_len = value->data.string->length;
      return SXC_SUCCESS;

    case sxc_cstring:
      *dest = value->data.cstring;
      *dest_len = strlen(value->data.cstring);
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


/* converting cstring arrays and SxcMaps into primitive arrays is straightforward
    but verbose, so we use macros to cover all the primitive array types */
/* NOTE these macros assume value, dest, and dest_len args, as well as tmp_value
    and i local variables */
/* NOTE an array or SxcMap filled with values that have nothing to do with the
    target primivitve type (e.g. non-numeric strings targeting doubles) will be
    naturally converted to an array filled with all zero values */

#define CSTRINGS2ARRAY(ELEMENT_TYPE, CONVERT_FUNC)                                \
  *dest_len = value->data.cstrings.length;                                        \
  *dest = sxc_context_alloc(value->context, sizeof(ELEMENT_TYPE) * (*dest_len));  \
  tmp_value.context = value->context;                                             \
  tmp_value.type = sxc_cstring;                                                   \
  for (i = 0; i < *dest_len; i += 1) {                                            \
    tmp_value.data.cstring = value->data.cstrings.array[i];                       \
    if (!CONVERT_FUNC(&tmp_value, &((*dest)[i]))) {                                \
      (*dest)[i] = (ELEMENT_TYPE)0;                                               \
    }                                                                             \
  }

#define MAP2ARRAY(ELEMENT_TYPE, CONVERT_FUNC)                                     \
  *dest_len = sxc_map_length(value->data.map);                                    \
  *dest = sxc_context_alloc(value->context, sizeof(ELEMENT_TYPE) * (*dest_len));  \
  tmp_value.context = value->context;                                             \
  for (i = 0; i < *dest_len; i += 1) {                                            \
    ((value->data.map)->binding->intget)(value->data.map, i, &tmp_value);          \
    if (!CONVERT_FUNC(&tmp_value, &((*dest)[i]))) {                                \
      (*dest)[i] = (ELEMENT_TYPE)0;                                               \
    }                                                                             \
  }


static int to_cbools(SxcValue* value, char** dest, int* dest_len) {
  SxcValue tmp_value;
  int i;

  switch (value->type) {
    case sxc_cbools:
      *dest_len = value->data.cbools.length;
      *dest = value->data.cbools.array;
      return SXC_SUCCESS;

    case sxc_cints:
      *dest_len = value->data.cints.length;
      *dest = sxc_context_alloc(value->context, sizeof(char) * (*dest_len));
      for (i = 0; i < *dest_len; i += 1) {
        (*dest)[i] = value->data.cints.array[i] != 0;
      }
      return SXC_SUCCESS;

    case sxc_cdoubles:
      *dest_len = value->data.cdoubles.length;
      *dest = sxc_context_alloc(value->context, sizeof(char) * (*dest_len));
      for (i = 0; i < *dest_len; i += 1) {
        (*dest)[i] = value->data.cdoubles.array[i] != 0.0;
      }
      return SXC_SUCCESS;

    case sxc_cstrings:
      CSTRINGS2ARRAY(char, to_bool)
      return SXC_SUCCESS;

    case sxc_map:
      MAP2ARRAY(char, to_bool)
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cints(SxcValue* value, int** dest, int* dest_len) {
  SxcValue tmp_value;
  int i;

  switch (value->type) {
    case sxc_cints:
      *dest_len = value->data.cints.length;
      *dest = value->data.cints.array;
      return SXC_SUCCESS;

    case sxc_cbools:
      *dest_len = value->data.cbools.length;
      *dest = sxc_context_alloc(value->context, sizeof(int) * (*dest_len));
      for (i = 0; i < *dest_len; i += 1) {
        (*dest)[i] = (int)value->data.cbools.array[i];
      }
      return SXC_SUCCESS;

    case sxc_cdoubles:
      *dest_len = value->data.cdoubles.length;
      *dest = sxc_context_alloc(value->context, sizeof(int) * (*dest_len));
      for (i = 0; i < *dest_len; i += 1) {
        (*dest)[i] = (int)value->data.cdoubles.array[i];
      }
      return SXC_SUCCESS;

    case sxc_cstrings:
      CSTRINGS2ARRAY(int, to_int)
      return SXC_SUCCESS;

    case sxc_map:
      MAP2ARRAY(int, to_int)
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


static int to_cdoubles(SxcValue* value, double** dest, int* dest_len) {
  SxcValue tmp_value;
  int i;

  switch (value->type) {
    case sxc_cdoubles:
      *dest_len = value->data.cdoubles.length;
      *dest = value->data.cdoubles.array;
      return SXC_SUCCESS;

    case sxc_cbools:
      *dest_len = value->data.cbools.length;
      *dest = sxc_context_alloc(value->context, sizeof(double) * (*dest_len));
      for (i = 0; i < *dest_len; i += 1) {
        (*dest)[i] = (double)value->data.cbools.array[i];
      }
      return SXC_SUCCESS;

    case sxc_cints:
      *dest_len = value->data.cints.length;
      *dest = sxc_context_alloc(value->context, sizeof(double) * (*dest_len));
      for (i = 0; i < *dest_len; i += 1) {
        (*dest)[i] = (double)value->data.cints.array[i];
      }
      return SXC_SUCCESS;

    case sxc_cstrings:
      CSTRINGS2ARRAY(double, to_double)
      return SXC_SUCCESS;

    case sxc_map:
      MAP2ARRAY(double, to_double)
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}


/* cleanup */
#undef CSTRINGS2ARRAY
#undef MAP2ARRAY


static int to_cstrings(SxcValue* value, char*** dest, int* dest_len) {
  SxcValue tmp_value;
  int i;

  switch (value->type) {
    case sxc_cstrings:
      *dest_len = value->data.cstrings.length;
      *dest = value->data.cstrings.array;
      return SXC_SUCCESS;

      /* converting primitive arrays into cstring array is yet another
          straightforward but verbose operation, so we use yet another macro */
      #define ARRAY2CSTRINGS(ARRAY_NAME, ELEMENT_TYPE, ELEMENT_DATA_NAME)       \
        *dest_len = value->data.ARRAY_NAME.length;                              \
        *dest = sxc_context_alloc(value->context, sizeof(char*) * (*dest_len)); \
        tmp_value.context = value->context;                                     \
        tmp_value.type = ELEMENT_TYPE;                                          \
        for (i = 0; i < *dest_len; i += 1) {                                    \
          tmp_value.data.ELEMENT_DATA_NAME = value->data.ARRAY_NAME.array[i];   \
          if (!to_cstring(&tmp_value, &((*dest)[i]))) {                          \
            (*dest)[i] = NULL;                                                  \
          }                                                                     \
        }

    case sxc_cbools:
      ARRAY2CSTRINGS(cbools, sxc_bool, abool)
      return SXC_SUCCESS;

    case sxc_cints:
      ARRAY2CSTRINGS(cints, sxc_int, aint)
      return SXC_SUCCESS;

    case sxc_cdoubles:
      ARRAY2CSTRINGS(cdoubles, sxc_double, adouble)
      return SXC_SUCCESS;

      /* cleanup */
      #undef ARRAY2CSTRINGS

    case sxc_map:
      *dest_len = sxc_map_length(value->data.map);
      *dest = sxc_context_alloc(value->context, sizeof(char*) * (*dest_len));
      tmp_value.context = value->context;
      for (i = 0; i < *dest_len; i += 1) {
        ((value->data.map)->binding->intget)(value->data.map, i, &tmp_value);

        if (!to_cstring(&tmp_value, &((*dest)[i]))) {
          (*dest)[i] = NULL;
        }
      }
      return SXC_SUCCESS;

    default:
      return SXC_FAILURE;
  }
}




/***** SxcValue Functions *****/

int sxc_value_getv(SxcValue* value, SxcDataType type, va_list varg) {
  /* these variables are necessary because evaluation of function arguments in C
      is not well-defined (god knows why), thus the va_arg macro gets confused
      when used as a to_XXXX() arg */
  void* dest;
  int* dest_len;

  if (type != sxc_null) {
    dest = va_arg(varg, void*);
  }

  switch (type) {
    case sxc_bool:
      return to_bool(value, (char*)dest);
    case sxc_int:
      return to_int(value, (int*)dest);
    case sxc_double:
      return to_double(value, (double*)dest);
    case sxc_string:
      return to_string(value, (SxcString**)dest);
    case sxc_map:
      return to_map(value, (SxcMap**)dest);
    case sxc_function:
      return to_function(value, (SxcFunction**)dest);
    case sxc_cstring:
      return to_cstring(value, (char**)dest);
    case sxc_cpointer:
      return to_cpointer(value, (void**)dest);
    case sxc_cfunction:
      return to_cfunction(value, (SxcLibFunction**)dest);

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
        /* TODO unrecognized/invalid type error */
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
      case sxc_bool:
        /* NOTE char var args are always promoted to int (see http://c-faq.com/varargs/float.html) */
        value->data.abool = (char)va_arg(varg, int); /* TODO? coerce to 0 or 1 */
      case sxc_int:
        value->data.aint = va_arg(varg, int);
        break;
      case sxc_double:
        value->data.adouble = va_arg(varg, double);
        break;

      case sxc_string:
      case sxc_map:
      case sxc_function:
      case sxc_cstring:
      case sxc_cpointer:
      case sxc_cfunction:
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
          /* TODO unrecognized/invalid type error */
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


void sxc_value_intern(SxcValue* value) {
  SxcData data;

  switch (value->type) {
    case sxc_cfunction:
      to_function(value, &(data.function));
      value->type = sxc_function;
      value->data.function = data.function;
      break;

    case sxc_cstring:
    case sxc_cpointer:
    case sxc_cchars:
      to_string(value, &(data.string));
      value->type = sxc_string;
      value->data.string = data.string;
      break;

    case sxc_cbools:
    case sxc_cints:
    case sxc_cdoubles:
    case sxc_cstrings:
      to_map(value, &(data.map));
      value->type = sxc_map;
      value->data.map = data.map;
      break;

    default:
      /* already interned */
      break;
  }
}
