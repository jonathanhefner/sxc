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


int sxc_map_intget(SxcMap* map, int key, bool is_required, SxcDataType type, SXC_DATA_DEST) {
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


int sxc_map_strget(SxcMap* map, const char* key, bool is_required, SxcDataType type, SXC_DATA_DEST) {
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
  SxcValue key;
  SxcValue val;
  int key_count = 0;
  int key_max = -1;

  if (map->binding->length != NULL) {
    return (map->binding->length)(map);
  }

  /* For bindings that don't provide a length() function, a length is computed
      based on the max integer key of the map.  However, if the map contains any
      data associated with string keys, or if the map is sparse (defined here as
      null to non-null element ratio of 8:1 or greater), a negative length is
      returned to indicate the map is a dictionary, rather than a list. */
  /* NOTE This is obviously not very performant.  Bindings should only rely on
      this feature when the length is not truly known, e.g. a hashmap (possibly)
      representing a sparse vector */
  else {
    while ((iter = sxc_map_iter(map, iter, &key, &val))) {
      /* TODO? account for stringified integer keys */
      if (key.type == sxc_string && val.type != sxc_func) {
        return -1;
      }

      if (key.type == sxc_cint) {
        key_count += 1;
        key_max = key.data.cint > key_max ? key.data.cint : key_max;
      }
    }

    if ((key_max - key_count) >= (8 * key_count)) {
      return -1;
    }

    return key_max + 1;
  }
}


void* sxc_map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value) {
  /* eliminate a potential source of errors */
  return_key->context = map->context;
  return_value->context = map->context;

  /* skip over keys that are not integers or strings, and return NULL when
      there's nothing left to iterate over */
  while ((state = (map->binding->iter)(map, state, return_key, return_value))) {
    switch (return_key->type) {
      case sxc_cint:
      case sxc_string:
        return state;

      /* handle when the scripting language has only a double numeric type and
          the binding doesn't convert to int when appropriate */
      case sxc_cdouble:
        if (return_key->data.cdouble == (double)(int)(return_key->data.cdouble)) {
          sxc_value_set(return_key, sxc_cint, (int)(return_key->data.cdouble));
          return state;
        }
      default:
        break;
    }
  }

  return NULL;
}
