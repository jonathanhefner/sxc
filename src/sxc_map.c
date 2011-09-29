#include "sxc.h"

void sxc_value_intern(SxcValue* value);


SxcMap* sxc_map_new(SxcContext* context, void* map_type) {
  return (context->binding->map_new)(context, map_type);
}

void* sxc_map_newtype(SxcContext* context, const char* name, SxcLibFunction initialzier,
                      const SxcLibMethod* methods, const SxcLibProperty* properties) {
  const SxcLibMethod no_methods[] = { {0} };
  const SxcLibProperty no_properties[] = { {0} };

  /* TODO check for name collisions within methods and properties */
  /* TODO? check for invalid characters in names */
  return (context->binding->map_newtype)(context, name, initialzier,
    methods == NULL ? no_methods : methods, properties == NULL ? no_properties : properties);
}

void sxc_map_intget_value(SxcMap* map, int key, SxcValue* return_value) {
  if (return_value->context == NULL) {
    return_value->context = map->context;
  }
  (map->binding->intget)(map, key, return_value);
}

void sxc_map_intset_value(SxcMap* map, int key, SxcValue* value) {
  if (value->context == NULL) {
    value->context = map->context;
  }
  sxc_value_intern(value);
  (map->binding->intset)(map, key, value);
}

void sxc_map_strget_value(SxcMap* map, char* key, SxcValue* return_value) {
  if (return_value->context == NULL) {
    return_value->context = map->context;
  }
  (map->binding->strget)(map, key, return_value);
}

void sxc_map_strset_value(SxcMap* map, char* key, SxcValue* value) {
  if (value->context == NULL) {
    value->context = map->context;
  }
  sxc_value_intern(value);
  (map->binding->strset)(map, key, value);
}

void* sxc_map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value) {
  if (return_key->context == NULL) {
    return_key->context = map->context;
  }
  if (return_value->context == NULL) {
    return_value->context = map->context;
  }
  return (map->binding->iter)(map, state, return_key, return_value);
}
