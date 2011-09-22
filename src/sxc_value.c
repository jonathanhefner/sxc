#include <math.h>
#include <string.h>
#include <stdio.h>
#include "sxc.h"

#ifndef isnan
  #define isnan(x) (x != x)
#endif


SxcValue** sxc_value_new(SxcContext* context, int count) {
  SxcValue** val_ptrs = sxc_context_alloc(context, count * (sizeof(SxcValue*) + sizeof(SxcValue)));
  SxcValue* vals = (SxcValue*)(val_ptrs + count);
  int i;

  for (i = 0; i < count; i += 1) {
    vals[i].context = context;
    val_ptrs[i] = &vals[i];
    sxc_value_set_null(val_ptrs[i]);
  }

  return val_ptrs;
}


/***** Getter Methods *****/

char sxc_value_get_boolean(SxcValue* value, const char default_value) {
  switch (value->type) {
    case sxc_type_boolean:
      return value->data.boolean;

    case sxc_type_integer:
      return (char)(value->data.integer);

    case sxc_type_decimal:
    case sxc_type_string:
      return sxc_value_get_decimal(value, (double)default_value) != 0.0;

    case sxc_type_null:
    case sxc_type_map:
    case sxc_type_function:
    default:
      return default_value;
  }
}


int sxc_value_get_integer(SxcValue* value, const int default_value) {
  switch (value->type) {
    case sxc_type_boolean:
      return (int)(value->data.boolean);

    case sxc_type_integer:
      return value->data.integer;

    case sxc_type_decimal:
    case sxc_type_string:
      return (int)(sxc_value_get_decimal(value, (double)default_value));

    case sxc_type_null:
    case sxc_type_map:
    case sxc_type_function:
    default:
      return default_value;
  }
}


/* NOTES:
    - NaN is treated the same as null (i.e. default_value is returned)
*/
double sxc_value_get_decimal(SxcValue* value, const double default_value) {
  double decimal;
  char decimal_format[32];

  switch (value->type) {
    case sxc_type_boolean:
      return (double)(value->data.boolean);

    case sxc_type_integer:
      return (double)(value->data.integer);

    case sxc_type_decimal:
      return isnan(value->data.decimal) ? default_value : value->data.decimal;

    case sxc_type_string:
      /* in case string is not null-terminated, we create a scanf format string
          that won't read past end of string */
      sprintf(decimal_format, "%%%dlf", value->data.string->length);

      return (sscanf(value->data.string->data, decimal_format, &decimal)
              && !isnan(decimal)) ? default_value : decimal;

    case sxc_type_null:
    case sxc_type_map:
    case sxc_type_function:
    default:
      return default_value;
  }
}


/*
  NOTES:
    - a NaN number value is treated the same as NULL (i.e. default_value value is returned)
    - if length is NULL, it will be ignored (not set)
    - if default_value is returned, *length will not be set (reason: unreliable to invoke strlen on default_value)
*/
SxcString* sxc_value_get_string(SxcValue* value, const SxcString* default_value) {
  char* str;
  int len;

printf("in sxc_value_get_string\n");

  switch (value->type) {
    case sxc_type_boolean:
      return sxc_string_new(value->context, value->data.boolean ? "1" : "0", 1, TRUE);

    case sxc_type_integer:
      /* format the number
       *    NOTE: max digits in a 32-bit signed int is 10 (not including sign)
       *    NOTE: max digits in a 64-bit signed int is 19 (not including sign) */
      str = sxc_context_alloc(value->context, (sizeof(int) <= 4 ? 11 : 20) + 1);
      len = sprintf(str, "%d", value->data.integer);
      return sxc_string_new(value->context, str, len, TRUE);

    case sxc_type_decimal:

printf("doing sxc_type_decimal\n");

      if (isnan(value->data.decimal)) {
        return (SxcString*)default_value;
      }

printf("done NaN check\n");

      /* format the number into no more than 20 characters (max digits in a 64-bit signed int is 19) */
      str = sxc_context_alloc(value->context, 20 + 1);

printf("done str alloc\n");

      /* try to format as long integer first, if there is no mantissa and the integer portion fits */
      if (value->data.decimal == (double)(long int)(value->data.decimal) && sizeof(long int) <= 8) {
printf("doing sprintf int\n");

        len = sprintf(str, "%ld", (long int)(value->data.decimal));
      } else {
printf("doing sprintf double\n");

        /* otherwise format as a floating point in scientific notation:
         *    |<-------20------->|
         *    +1.000000000000e+000
         *       |<---12--->|
         */
        len = sprintf(str, "%1.12e", value->data.decimal);
      }
printf("done sprintf, creating string\n");
      return sxc_string_new(value->context, str, len, TRUE);

    case sxc_type_string:
      return value->data.string;

    case sxc_type_null:
    case sxc_type_map:
    case sxc_type_function:
    default:
      return (SxcString*)default_value;
  }
}


SxcMap* sxc_value_get_map(SxcValue* value, const SxcMap* default_value) {
  switch (value->type) {
    case sxc_type_map:
      return value->data.map;

    case sxc_type_null:
    case sxc_type_boolean:
    case sxc_type_integer:
    case sxc_type_decimal:
    case sxc_type_string:
    case sxc_type_function: /* TODO turn function into a read-only map? */
    default:
      return (SxcMap*)default_value; /* TODO use special constant default_value to denote wrap in singleton list? */
  }
}


SxcFunction* sxc_value_get_function(SxcValue* value, const SxcFunction* default_value) {
  switch (value->type) {
    case sxc_type_function:
      return value->data.function;

    case sxc_type_null:
    case sxc_type_boolean:
    case sxc_type_integer:
    case sxc_type_decimal:
    case sxc_type_string:
    case sxc_type_map: /* TODO turn map into a function? */
    default:
      return (SxcFunction*)default_value;
  }
}




/***** Setter Methods *****/

void sxc_value_set_null(SxcValue* value) {
  value->type = sxc_type_null;
}

void sxc_value_set_boolean(SxcValue* value, char boolean) {
  value->type = sxc_type_boolean;
  value->data.boolean = boolean; /* TODO coerce to 0 or 1? */
}

void sxc_value_set_integer(SxcValue* value, int integer) {
  value->type = sxc_type_integer;
  value->data.integer = integer;
}

void sxc_value_set_decimal(SxcValue* value, double decimal) {
  value->type = sxc_type_decimal;
  value->data.decimal = decimal;
}

void sxc_value_set_string(SxcValue* value, SxcString* string) {
  value->type = string == NULL ? sxc_type_null : sxc_type_string;
  value->data.string = string;
}

void sxc_value_set_map(SxcValue* value, SxcMap* map) {
  value->type = map == NULL ? sxc_type_null : sxc_type_map;
  value->data.map = map;
}

void sxc_value_set_function(SxcValue* value, SxcFunction* function) {
  value->type = function == NULL ? sxc_type_null : sxc_type_function;
  value->data.function = function;
}




/***** Package-Level Methods *****/

void sxc_value_intern(SxcValue* value) {
  /* intern strings as necessary */
  if (value->type == sxc_type_string && value->data.string->underlying == NULL) {
    value->data.string = sxc_string_new(value->context, value->data.string->data, value->data.string->length, FALSE);
  }
}
