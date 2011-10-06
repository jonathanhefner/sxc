#ifndef NULL
  #define NULL ((void*)0)
#endif

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE (!FALSE)
#endif



/***** General Types *****/

#define SXC_SUCCESS (1)
#define SXC_FAILURE (0)

typedef enum _SxcDataType SxcDataType;
typedef struct _SxcValue SxcValue;
typedef struct _SxcString SxcString;
typedef struct _SxcMap SxcMap;
typedef struct _SxcFunction SxcFunction;
typedef struct _SxcContext SxcContext;

#define MAPTYPE_HASH (NULL)
#define MAPTYPE_LIST ((void*)1)



/***** Library Types *****/

typedef void (SxcLibFunction)(SxcContext* context, SxcValue* return_value);

typedef struct _SxcLibMethod {
  char* name;
  int is_static;
  SxcLibFunction* function;

} SxcLibMethod;

typedef struct _SxcLibProperty {
  char* name;
  int is_static;
  SxcLibFunction* getter;
  SxcLibFunction* setter;
} SxcLibProperty;



/***** Binding Types *****/

typedef struct _SxcMapBinding {
  void (*intget)(SxcMap* map, int key, SxcValue* return_value);
  void (*intset)(SxcMap* map, int key, SxcValue* value);

  void (*strget)(SxcMap* map, const char* key, SxcValue* return_value);
  void (*strset)(SxcMap* map, const char* key, SxcValue* value);

  int (*length)(SxcMap* map);
  void* (*iter)(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value);
} SxcMapBinding;


typedef struct _SxcFunctionBinding {
  void (*invoke)(SxcFunction* function, SxcValue** args, int argcount, SxcValue* return_value);
  /* TODO serialize(), deserialize() */
} SxcFunctionBinding;


typedef struct _SxcContextBinding {
  void (*get_arg)(SxcContext* context, int index, SxcValue* return_value);
  SxcString* (*string_intern)(SxcContext* context, const char* data, int length);
  void* (*map_newtype)(SxcContext* context, const char* name,
          SxcLibFunction initializer, const SxcLibMethod* methods, const SxcLibProperty* properties);
  SxcMap* (*map_new)(SxcContext* context, void* map_type);
  SxcFunction* (*function_wrap)(SxcContext* context, SxcLibFunction func);

} SxcContextBinding;



/***** General Prototypes *****/

#define SXC_DATA_ARG ...
#define SXC_DATA_DEST ...
#define SXC_DATA_DEST_ARGS ...

void* sxc_context_alloc(SxcContext* context, int size);
void* sxc_context_error(SxcContext* context, const char* message_format, ...);
int sxc_context_arg(SxcContext* context, int index, SxcDataType type, SXC_DATA_DEST);

int sxc_value_get(SxcValue* value, SxcDataType type, SXC_DATA_DEST);
void sxc_value_set(SxcValue* value, SxcDataType type, SXC_DATA_ARG);
void sxc_value_intern(SxcValue* value);

SxcString* sxc_string_new(SxcContext* context, char* data, int length);

SxcMap* sxc_map_new(SxcContext* context, void* map_type);
void* sxc_map_newtype(SxcContext* context, const char* name, SxcLibFunction initialzier,
                      const SxcLibMethod* methods, const SxcLibProperty* properties);
int sxc_map_intget(SxcMap* map, int key, SxcDataType type, SXC_DATA_DEST);
void sxc_map_intset(SxcMap* map, int key, SxcDataType type, SXC_DATA_ARG);
int sxc_map_strget(SxcMap* map, const char* key, SxcDataType type, SXC_DATA_DEST);
void sxc_map_strset(SxcMap* map, const char* key, SxcDataType type, SXC_DATA_ARG);
int sxc_map_length(SxcMap* map);
void* sxc_map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value);

SxcFunction* sxc_function_new(SxcContext* context, SxcLibFunction func);
int sxc_function_invoke(SxcFunction* function, int argcount, SxcDataType return_type, SXC_DATA_DEST_ARGS);



/***** Binding Prototypes *****/

void sxc_load(SxcContext* context, SxcValue* return_namespace);
SxcValue* sxc_context_try(SxcContext* context,  SxcContextBinding* binding, void* underlying, int argcount, SxcLibFunction func);
void sxc_context_finally(SxcContext* context);



/***** Type Definitions *****/

enum _SxcDataType {
  /* TODO? would these enum values be more usable/obvious as ALL CAPS? */

  /* These are the primitive types.  They should be represented similarly
      between C and any scripting language. */
  sxc_null,
  sxc_bool,
  sxc_int,
  sxc_double,

  /* These are the script types.  Each scripting language has their own, perhaps
      multiple, implementation of them.  Libraries interact with these types
      though sxc_[string|map|function] methods. */
  sxc_string,
  sxc_map,
  sxc_function,

  /* These are the C types.  Each is represented by a common C data type (plus
      an int length, in some cases), and can be converted to one of the script
      types using sxc_value_intern().  Language bindings should not worry about
      these types, as any data the binding recieves will already be converted
      to a script type. */
  sxc_cstring,   /* char* <=> SxcString */
  sxc_cpointer,  /* void* <=> SxcString */
  sxc_cfunction, /* SxcLibFunction => SxcFunction */
  sxc_cchars,    /* char* + int length <=> SxcString */
  sxc_cbools,    /* char* + int length <=> SxcMap */
  sxc_cints,     /* int* + int length <=> SxcMap */
  sxc_cdoubles,  /* double* + int length <=> SxcMap */
  sxc_cstrings   /* char** + int length <=> SxcMap */

  /* These are the meta types.  They don't represent actual data types, but add
      capability to the value type system. */
  #define sxc_value (-1)
          /* indicates that the associated data is a SxcValue* and the actual
              data should be extracted from that struct */
};


typedef union _SxcData {
  /* TODO better convention for making these not keywords? (boolval? intd? ddouble?) */
  char abool;
  int aint;
  double adouble;

  /* HACK _pointer_store can be used to generically read/write to the pointer
      members below (due to the nature of C unions) */
  void* _pointer_store;

  SxcString* string;
  SxcMap* map;
  SxcFunction* function;

  char* cstring;
  SxcLibFunction* cfunction;
  void* cpointer;

  /* HACK _array_store can be used to generically read/write to the array-like
      members below (due to the nature of C unions) */
  struct { void* array; int length; } _array_store;

  struct {
    char* array;
    int length;
  } cchars;

  struct {
    char* array;
    int length;
  } cbools;

  struct {
    int* array;
    int length;
  } cints;

  struct {
    double* array;
    int length;
  } cdoubles;

  struct {
    char** array;
    int length;
  } cstrings;
} SxcData;


struct _SxcValue {
  SxcContext* context;
  SxcDataType type;
  SxcData data;
};


struct _SxcString {
  SxcContext* context;
  void* underlying;

  char* data;
  int length;
  int is_terminated;
};


struct _SxcMap {
  SxcContext* context;
  SxcMapBinding* binding;
  void* underlying;

  int is_list;
};


struct _SxcFunction {
  SxcContext* context;
  SxcFunctionBinding* binding;
  void* underlying;
};


typedef struct _SxcMemoryChunk {
  int free_space;
  struct _SxcMemoryChunk* next_chunk;

  int offset;
  char data;
  /* REST OF DATA ALLOCATED BEYOND THIS POINT */
} SxcMemoryChunk;


#define SXC_MEMORY_CHUNK_INIT_SIZE (1024 * 2)
#define SXC_MEMORY_CHUNK_MAX_SIZE (1024 * 16)

struct _SxcContext {
  /* public */
  SxcContextBinding* binding;
  void* underlying;
  int argcount;
  SxcValue return_value;
  int has_error;

  /* private */
  void* _jmpbuf;
  SxcMemoryChunk _firstchunk;
  char _firstdata[SXC_MEMORY_CHUNK_INIT_SIZE - 1];
};
