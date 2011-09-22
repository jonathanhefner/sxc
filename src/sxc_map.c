#include "sxc.h"

void sxc_value_intern(SxcValue* value);


SxcMap* sxc_map_new(SxcContext* context, int is_list) {
  return (context->methods->map_new)(context, is_list);
}

void sxc_map_intget_value(SxcMap* map, int key, SxcValue* return_value) {
  if (return_value->context == NULL) {
    return_value->context = map->context;
  }
  (map->methods->intget)(map, key, return_value);
}

void sxc_map_intset_value(SxcMap* map, int key, SxcValue* value) {
  if (value->context == NULL) {
    value->context = map->context;
  }
  sxc_value_intern(value);
  (map->methods->intset)(map, key, value);
}

void sxc_map_strget_value(SxcMap* map, char* key, SxcValue* return_value) {
  if (return_value->context == NULL) {
    return_value->context = map->context;
  }
  (map->methods->strget)(map, key, return_value);
}

void sxc_map_strset_value(SxcMap* map, char* key, SxcValue* value) {
  if (value->context == NULL) {
    value->context = map->context;
  }
  sxc_value_intern(value);
  (map->methods->strset)(map, key, value);
}

void* sxc_map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value) {
  if (return_key->context == NULL) {
    return_key->context = map->context;
  }
  if (return_value->context == NULL) {
    return_value->context = map->context;
  }
  return (map->methods->iter)(map, state, return_key, return_value);
}
