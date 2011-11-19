/***** Some Prerequisites *****/

#include <stddef.h>

#if __STDC_VERSION__ >= 199901L
  #include <stdbool.h>
#else
  #define bool char
  #define true 1
  #define false 0
#endif



/***** General Types *****/

#define SXC_SUCCESS (1)
#define SXC_FAILURE (0)

typedef enum _SxcDataType SxcDataType;
typedef struct _SxcValue SxcValue;
typedef struct _SxcString SxcString;
typedef struct _SxcMap SxcMap;
typedef struct _SxcFunc SxcFunc;
typedef struct _SxcContext SxcContext;

#define MAPTYPE_HASH (NULL)
#define MAPTYPE_LIST ((void*)1)



/***** Library Types *****/

typedef void (SxcLibFunc)(SxcContext* context);

typedef struct _SxcLibMethod {
  char* name;
  int is_static;
  SxcLibFunc* func;

} SxcLibMethod;

typedef struct _SxcLibProperty {
  char* name;
  int is_static;
  SxcLibFunc* getter;
  SxcLibFunc* setter;
} SxcLibProperty;



/***** Binding Types *****/

typedef struct _SxcStringBinding {
  void (*to_cchars)(void* underlying, SxcValue* return_value);

  int is_null_terminated;
} SxcStringBinding;


typedef struct _SxcMapBinding {
  void (*intget)(void* underlying, int key, SxcValue* return_value);
  void (*intset)(void* underlying, int key, SxcValue* value);

  void (*strget)(void* underlying, const char* key, SxcValue* return_value);
  void (*strset)(void* underlying, const char* key, SxcValue* value);

  int (*length)(void* underlying);
  void* (*iter)(void* underlying, void* state, SxcValue* return_key, SxcValue* return_value);
} SxcMapBinding;


typedef struct _SxcFunctionBinding {
  void (*invoke)(void* underlying, SxcValue** args, int argcount, SxcValue* return_value);
} SxcFuncBinding;


typedef struct _SxcContextBinding {
  void (*get_arg)(void* underlying, int index, SxcValue* return_value);

  void (*to_sstring)(const char* data, int length, SxcValue* return_value);

  void (*map_new)(void* map_type, SxcValue* return_value);
  void* (*map_newtype)(SxcContext* context, const char* name, SxcLibFunc* initializer,
                        const SxcLibMethod* methods, const SxcLibProperty* properties);

  void (*to_sfunc)(SxcLibFunc* func, SxcValue* return_value);
} SxcContextBinding;



/***** General Prototypes *****/

#define SXC_DATA_ARG ...
#define SXC_DATA_DEST ...
#define SXC_DATA_DEST_ARGS ...

void* sxc_alloc(SxcContext* context, int size);
void* sxc_error(SxcContext* context, const char* message_format, ...);
int sxc_arg(SxcContext* context, int index, bool is_required, SxcDataType type, SXC_DATA_DEST);
void sxc_return(SxcContext* context, SxcDataType type, SXC_DATA_ARG);

int sxc_value_get(SxcValue* value, SxcDataType type, SXC_DATA_DEST);
void sxc_value_set(SxcValue* value, SxcDataType type, SXC_DATA_ARG);

SxcMap* sxc_map_new(SxcContext* context, void* map_type);
void* sxc_map_newtype(SxcContext* context, const char* name, SxcLibFunc initialzier,
                      const SxcLibMethod* methods, const SxcLibProperty* properties);
int sxc_map_intget(SxcMap* map, int key, bool is_required, SxcDataType type, SXC_DATA_DEST);
void sxc_map_intset(SxcMap* map, int key, SxcDataType type, SXC_DATA_ARG);
int sxc_map_strget(SxcMap* map, const char* key, bool is_required, SxcDataType type, SXC_DATA_DEST);
void sxc_map_strset(SxcMap* map, const char* key, SxcDataType type, SXC_DATA_ARG);
int sxc_map_length(SxcMap* map);
void* sxc_map_iter(SxcMap* map, void* state, SxcValue* return_key, SxcValue* return_value);

void sxc_func_invoke(SxcFunc* func, int argcount, SxcDataType return_type, SXC_DATA_DEST_ARGS);



/***** Binding Prototypes *****/

void sxc_load(SxcContext* context);
void sxc_try(SxcContext* context, void* underlying, SxcContextBinding* binding, int argcount, SxcLibFunc func);
void sxc_finally(SxcContext* context);



/***** Type Definitions *****/

enum _SxcDataType {
  /* These are the Primitive Types.  They should be represented similarly
      between C and any scripting language. */
  sxc_null,
  sxc_cbool,
  sxc_cint,
  sxc_cdouble,

  /* C LIBRARIES ONLY: These are the Interface Types.  They represent SxcString,
      SxcMap, and SxcFunc, respectively.  Non-primitive values that come from
      the scripting environment will be one of these types, unless other types
      are specifically requested (see C Types below). */
  sxc_string,
  sxc_map,
  sxc_func,

  /* LANGUAGE BINDINGS ONLY: These are the Script Types.  Language bindings
      passing non-primitive values to and from the C environment will use these
      types instead of interacting with sxc_string, sxc_map, and sxc_func
      directly.  Each type is represented by a void pointer to an arbitrary
      underlying data structure followed by a pointer to an appropriate
      Sxc*****Binding struct.

      Note: there is one exception to this.  Language bindings need to use the
      C Convenience Type sxc_cchars (see next section) to implement the
      SxcStringBinding.to_cchars() function. */
  sxc_sstring,   /* void* + SxcStringBinding* <=> SxcString* */
  sxc_smap,      /* void* + SxcMapBinding* <=> SxcMap* */
  sxc_sfunc,     /* void* + SxcFuncBinding* <=> SxcFunc* */

  /* C LIBRARIES ONLY: These are the C Convenience Types.  They are for passing
      non-primitive values to and from the scripting environment.  Each is
      represented by a common C data type, plus an int length in some cases. */
  sxc_cstring,   /* char* <=> SxcString* */
  sxc_cpointer,  /* void* <=> SxcString* */
  sxc_cfunc,     /* SxcLibFunc* => SxcFunc* */
  sxc_cchars,    /* char* + int length <=> SxcString* */
  sxc_cbools,    /* char* + int length <=> SxcMap* */
  sxc_cints,     /* int* + int length <=> SxcMap* */
  sxc_cdoubles,  /* double* + int length <=> SxcMap* */
  sxc_cstrings   /* char** + int length <=> SxcMap* */

  /* C LIBRARIES ONLY: These are the meta types.  They don't represent actual
      data types, but add capability to the value type system. */
  #define sxc_value (-1)
          /* indicates that the associated data is a SxcValue* and the actual
              data should be extracted from the referenced struct */
};


struct _SxcString {
  void* underlying;
  SxcStringBinding* binding;
  SxcContext* context;

  char* data;
  int length;
};


struct _SxcMap {
  void* underlying;
  SxcMapBinding* binding;
  SxcContext* context;
};


struct _SxcFunc {
  void* underlying;
  SxcFuncBinding* binding;
  SxcContext* context;
};


typedef union _SxcData {
  bool cbool;
  int cint;
  double cdouble;

  /* HACK _pointer_store can be used to generically read/write to the pointer
      members below (due to the nature of C unions) */
  void* _pointer_store;

  SxcString* string;
  SxcMap* map;
  SxcFunc* func;

  /* HACK _binding_store can be used to generically read/write to the script
      type members below (due to the nature of C unions) */
  struct { void* underlying; void* binding; } _binding_store;

  struct {
    void* underlying;
    SxcStringBinding* binding;
  } sstring;

  struct {
    void* underlying;
    SxcMapBinding* binding;
  } smap;

  struct {
    void* underlying;
    SxcFuncBinding* binding;
  } sfunc;

  char* cstring;
  SxcLibFunc* cfunc;
  void* cpointer;

  /* HACK _array_store can be used to generically read/write to the array-like
      members below (due to the nature of C unions) */
  struct { void* array; int length; } _array_store;

  struct {
    char* array;
    int length;
  } cchars;

  struct {
    bool* array;
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
  void* underlying;
  SxcContextBinding* binding;
  int argcount;
  SxcValue return_value;
  int has_error;

  /* private */
  void* _jmpbuf;
  SxcMemoryChunk _firstchunk;
  char _firstdata[SXC_MEMORY_CHUNK_INIT_SIZE - 1];
};
