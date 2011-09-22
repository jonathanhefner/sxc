#ifndef NULL
  #define NULL ((void*)0)
#endif

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE (!FALSE)
#endif




/***** Type Definitions *****/

typedef enum _SxcDataType {
  sxc_type_null,
  sxc_type_boolean,
  sxc_type_integer,
  sxc_type_decimal,
  sxc_type_string,
  sxc_type_map,
  sxc_type_function
} SxcDataType;


typedef struct _SxcValue {
  struct _SxcContext* context;
  SxcDataType type;

  union {
    char boolean;
    int integer;
    double decimal;
    struct _SxcString* string;
    struct _SxcMap* map;
    struct _SxcFunction* function;
  } data;
} SxcValue;


typedef struct _SxcString {
  struct _SxcContext* context;
  void* underlying;

  char* data;
  int length;
} SxcString;


typedef struct _SxcMap {
  struct _SxcContext* context;
  struct _SxcMapMethods* methods;
  void* underlying;

  int is_list;
} SxcMap;


typedef struct _SxcFunction {
  struct _SxcContext* context;
  struct _SxcFunctionMethods* methods;
  void* underlying;
} SxcFunction;


typedef struct _SxcMemoryChunk {
  int free_space;
  struct _SxcMemoryChunk* next_chunk;

  int offset;
  char data;
  /* REST OF DATA ALLOCATED BEYOND THIS POINT */
} SxcMemoryChunk;


#define SXC_MEMORY_CHUNK_INIT_SIZE (1024 * 2)
#define SXC_MEMORY_CHUNK_MAX_SIZE (1024 * 16)

typedef struct _SxcContext {
  /* public */
  struct _SxcContextMethods* methods;
  void* underlying;
  int argcount;
  struct _SxcValue return_value;
  int has_error;

  /* private */
  void* _jmpbuf;
  struct _SxcValue _tmpval;
  struct _SxcMemoryChunk _firstchunk;
  char _firstdata[SXC_MEMORY_CHUNK_INIT_SIZE - 1];
} SxcContext;


typedef void (*SxcApi)(SxcContext* context, SxcValue* return_value);



/***** Method Table Definitions *****/

typedef struct _SxcMapMethods {
  void (*intget)(SxcMap* map, int key, SxcValue* return_value);
  void (*intset)(SxcMap* map, int key, SxcValue* value);

  void (*strget)(SxcMap* map, char* key, SxcValue* return_value);
  void (*strset)(SxcMap* map, char* key, SxcValue* value);

  void* (*iter)(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value);
} SxcMapMethods;


typedef struct _SxcFunctionMethods {
  void (*invoke)(SxcFunction* function, SxcValue** args, int argcount, SxcValue* return_value);
  /* TODO serialize(), deserialize() */
} SxcFunctionMethods;


typedef struct _SxcContextMethods {
  void (*get_arg)(SxcContext* context, int index, SxcValue* return_value);
  SxcString* (*string_intern)(SxcContext* context, const char* data, int length);
  SxcMap* (*map_new)(SxcContext* context, short int is_list);
  SxcFunction* (*function_wrap)(SxcContext* context, SxcApi api);
} SxcContextMethods;



/***** Function Prototypes *****/

void sxc_load(SxcContext* context, SxcValue* return_namespace);

void* sxc_context_alloc(SxcContext* context, int size);
void* sxc_context_error(SxcContext* context, char* message_format, ...);
void sxc_context_get_arg(SxcContext* context, int index, SxcValue* return_value);
SxcValue* sxc_context_try(SxcContext* context,  SxcContextMethods* methods, void* underlying, int argcount, SxcApi api);
void sxc_context_finally(SxcContext* context);

SxcValue** sxc_value_new(SxcContext* context, int count);
char sxc_value_get_boolean(SxcValue* value, const char default_value);
int sxc_value_get_integer(SxcValue* value, const int default_value);
double sxc_value_get_decimal(SxcValue* value, const double default_value);
SxcString* sxc_value_get_string(SxcValue* value, const SxcString* default_value);
SxcMap* sxc_value_get_map(SxcValue* value, const SxcMap* default_value);
SxcFunction* sxc_value_get_function(SxcValue* value, const SxcFunction* default_value);
void sxc_value_set_null(SxcValue* value);
void sxc_value_set_boolean(SxcValue* value, char boolean);
void sxc_value_set_integer(SxcValue* value, int integer);
void sxc_value_set_decimal(SxcValue* value, double decimal);
void sxc_value_set_string(SxcValue* value, SxcString* string);
void sxc_value_set_map(SxcValue* value, SxcMap* map);
void sxc_value_set_function(SxcValue* value, SxcFunction* function);

SxcString* sxc_string_new(SxcContext* context, char* data, int length, int lazy);

SxcMap* sxc_map_new(SxcContext* context, int is_list);
void sxc_map_intget_value(SxcMap* map, int key, SxcValue* return_value);
void sxc_map_intset_value(SxcMap* map, int key, SxcValue* value);
void sxc_map_strget_value(SxcMap* map, char* key, SxcValue* return_value);
void sxc_map_strset_value(SxcMap* map, char* key, SxcValue* value);
void* sxc_map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value);

SxcFunction* sxc_function_new(SxcContext* context, SxcApi api);
void sxc_function_invoke(SxcFunction* function, SxcValue** args, int argcount, SxcValue* return_value);



/***** Convenience Macros *****/

/* these are ancillary to the actual convenience macros */
#define sxc_valget__boolean(value, default_data) (sxc_value_get_boolean(value, default_data))
#define sxc_valget__integer(value, default_data) (sxc_value_get_integer(value, default_data))
#define sxc_valget__decimal(value, default_data) (sxc_value_get_decimal(value, default_data))
#define sxc_valget__string(value, default_data) (sizeof(*(default_data)) == sizeof(char) ? \
    sxc_value_get_string(value, sxc_string_new((value)->context, (char*)(default_data), -1, TRUE)) \
    : sxc_value_get_string(value, (SxcString*)(default_data)))
#define sxc_valget__map(value, default_data) (sxc_value_get_map(value, default_data))
#define sxc_valget__function(value, default_data) (sizeof(*(default_data)) == sizeof(*(SxcApi)sxc_load) ? \
    sxc_value_get_function(value, sxc_function_new((value)->context, (SxcApi)(default_data))) \
    : sxc_value_get_function(value, (SxcFunction*)(default_data)))


/* these are ancillary to the actual convenience macros */
#define sxc_valset__null(value, data) (sxc_value_set_null(value))
#define sxc_valset__boolean(value, data) (sxc_value_set_boolean(value, data))
#define sxc_valset__integer(value, data) (sxc_value_set_integer(value, data))
#define sxc_valset__decimal(value, data) (sxc_value_set_decimal(value, data))
#define sxc_valset__string(value, data) (sizeof(*(data)) == sizeof(char) ? \
    sxc_value_set_string(value, sxc_string_new((value)->context, (char*)(data), -1, FALSE)) \
    : sxc_value_set_string(value, (SxcString*)(data)))
#define sxc_valset__map(value, data) (sxc_value_set_map(value, data))
#define sxc_valset__function(value, data) (sizeof(*(data)) == sizeof(*(SxcApi)sxc_load) ? \
    sxc_value_set_function(value, sxc_function_new((value)->context, (SxcApi)(data))) \
    : sxc_value_set_function(value, (SxcFunction*)(data)))


#define sxc_valget(/*(SxcValue*)*/ value, DATA_TYPE, default_data) \
  (sxc_valget__##DATA_TYPE(value, default_data))

#define sxc_valset(/*(SxcValue*)*/ value, DATA_TYPE, data) \
  (sxc_valset__##DATA_TYPE(value, data))


#define sxc_intget(/*(SxcContext*|SxcMap*)*/ container, /*(int)*/ key, DATA_TYPE, default_data) \
  (sizeof(*(container)) == sizeof(SxcContext) ? \
      (sxc_context_get_arg((SxcContext*)(container), key, &((SxcContext*)(container))->_tmpval), \
        sxc_valget(&((SxcContext*)(container))->_tmpval, DATA_TYPE, default_data)) \
      : (sxc_map_intget_value((SxcMap*)(container), key, &((SxcMap*)(container))->context->_tmpval), \
          sxc_valget(&((SxcMap*)(container))->context->_tmpval, DATA_TYPE, default_data)))

#define sxc_intset(/*(SxcMap*)*/ map, /*(int)*/ key, DATA_TYPE, data) \
  do { \
    sxc_valset(&(map)->context->_tmpval, DATA_TYPE, data); \
    sxc_map_intset_value(map, key, &(map)->context->_tmpval); \
  } while (0);


/* TODO? support named args from contexts a la Ruby (extract from last-arg map) */
#define sxc_strget(/*(SxcMap*)*/ map, /*(char*)*/ key, DATA_TYPE, default_data) \
  (sizeof(*(container)) == sizeof(SxcContext) ? \
      (sxc_value_set_null(&((SxcContext*)(container))->_tmpval) \
        sxc_valset(&((SxcContext*)(container))->_tmpval, DATA_TYPE, default_data)) \
      : (sxc_map_strget_value((SxcMap*)(container), key, &((SxcMap*)(container))->context->_tmpval), \
          sxc_valset(&((SxcMap*)(container))->context->_tmpval, DATA_TYPE, default_data)))

#define sxc_strset(/*(SxcMap*)*/ map, /*(char*)*/ key, DATA_TYPE, data) \
  do { \
    sxc_valset(&(map)->context->_tmpval, DATA_TYPE, data); \
    sxc_map_strset_value(map, key, &(map)->context->_tmpval); \
  } while (0);
