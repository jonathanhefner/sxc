#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "sxc.h"

void sxc_typeerror(SxcContext* context, char* value_name, SxcDataType expected_type, SxcValue* actual_value);
int sxc_value_getv(SxcValue* value, SxcDataType type, va_list varg);
void sxc_value_setv(SxcValue* value, SxcDataType type, va_list varg);
void sxc_value_intern(SxcValue* value);



SxcMap* sxc_map_new(SxcContext* context, void* map_type) {
  return (context->binding->map_new)(context, map_type);
}

void* sxc_map_newtype(SxcContext* context, const char* name, SxcLibFunc initialzier,
                      const SxcLibMethod* methods, const SxcLibProperty* properties) {
  const SxcLibMethod no_methods[] = { {0} };
  const SxcLibProperty no_properties[] = { {0} };

  /* TODO check for name collisions within methods and properties */
  /* TODO? check for invalid characters in names */
  return (context->binding->map_newtype)(context, name, initialzier,
    methods == NULL ? no_methods : methods, properties == NULL ? no_properties : properties);
}


int sxc_map_intget(SxcMap* map, int key, int is_required, SxcDataType type, SXC_DATA_DEST) {
  va_list varg;
  int retval;
  SxcValue value;

  const char* value_name_format = "element %d";
  char* value_name;

  value.context = map->context;
  (map->binding->intget)(map, key, &value);

  va_start(varg, type);
  retval = sxc_value_getv(&value, type, varg);
  va_end(varg);

  if (is_required && retval != SXC_SUCCESS) {
    value_name = sxc_alloc(map->context, sizeof(char) * (strlen(value_name_format) + 20 + 1));
    sprintf(value_name, value_name_format, key);

    sxc_typeerror(map->context, value_name, type, &value);
  }
  return retval;
}

void sxc_map_intset(SxcMap* map, int key, SxcDataType type, SXC_DATA_ARG) {
  va_list varg;
  SxcValue value;

  value.context = map->context;
  va_start(varg, type);
  sxc_value_setv(&value, type, varg);
  va_end(varg);

  sxc_value_intern(&value);
  (map->binding->intset)(map, key, &value);
}


int sxc_map_strget(SxcMap* map, const char* key, int is_required, SxcDataType type, SXC_DATA_DEST) {
  va_list varg;
  int retval;
  SxcValue value;

  const char* value_name_format = "element \"%s\"";
  char* value_name;

  value.context = map->context;
  (map->binding->strget)(map, key, &value);

  va_start(varg, type);
  retval = sxc_value_getv(&value, type, varg);
  va_end(varg);

  if (is_required && retval != SXC_SUCCESS) {
    value_name = sxc_alloc(map->context, sizeof(char) * (strlen(value_name_format) + strlen(key) + 1));
    sprintf(value_name, value_name_format, key);

    sxc_typeerror(map->context, value_name, type, &value);
  }
  return retval;
}

void sxc_map_strset(SxcMap* map, const char* key, SxcDataType type, SXC_DATA_ARG) {
  va_list varg;
  SxcValue value;

  value.context = map->context;
  va_start(varg, type);
  sxc_value_setv(&value, type, varg);
  va_end(varg);

  sxc_value_intern(&value);
  (map->binding->strset)(map, key, &value);
}


int sxc_map_length(SxcMap* map) {
  void* iter = NULL;
  SxcValue map_key;
  SxcValue map_val;
  int max_int_key = -1;
  int length = (map->binding->length)(map);

  /* for convenience, bindings can return a length less than zero, and we will
      iterate through the map to compute (max_int_key + 1) as the length */
  /* NOTE this is obviously not very performant.  Bindings should only rely on
      this feature when the length is not truly known, e.g. a hashmap (possibly)
      representing a sparse vector */
  if (length < 0) {
    while ((iter = sxc_map_iter(map, iter, &map_key, &map_val))) {
      if (map_key.type == sxc_int) {
        max_int_key = map_key.data.aint > max_int_key ? map_key.data.aint : max_int_key;
      }
    }

    length = max_int_key + 1;
  }

  return length;
}


void* sxc_map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value) {
  void* next_state;

  /* eliminate a potential source of errors */
  return_key->context = map->context;
  return_value->context = map->context;

  /* skip over keys that are not integers or strings, and return NULL when
      there's nothing left to iterate over */
  while ((next_state = (map->binding->iter)(map, state, return_key, return_value))) {
    switch (return_key->type) {
      case sxc_int:
      case sxc_string:
        return next_state;

      /* handle when the scripting language has only a double numeric type and
          the binding doesn't convert to int when appropriate */
      case sxc_double:
        if (return_key->data.adouble == (double)(int)(return_key->data.adouble)) {
          return_key->data.aint = (int)(return_key->data.adouble);
          return next_state;
        }

      default:
        break;
    }
  }

  return NULL;
}
