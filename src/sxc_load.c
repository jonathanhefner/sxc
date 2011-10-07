#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sxc.h"


#define SXC_LIB_REGISTER_FUNC "sxc_lib_register"

/* Much of the code here is inspired by Lua's dynamic library loader, which can
    be found at http://www.lua.org/source/5.1/loadlib.c.html */


static char* fix_lib_name(SxcContext* context, const char* lib_name, int lib_name_len, const char* name_format, char slash) {
  const int name_format_len = strlen(name_format);
  char* lib_name_fixed = sxc_alloc(context, lib_name_len + name_format_len + 1);
  int i = lib_name_len - 1;
  int last_slash;

  while (i >= 0 && lib_name[i] != '/' && lib_name[i] != '\\') {
    i -= 1;
  }
  last_slash = i;

  for (; i >= 0; i -= 1) {
    lib_name_fixed[i] = (lib_name[i] == '\\' || lib_name[i] == '/') ? slash : lib_name[i];
  }

  sprintf(&lib_name_fixed[last_slash + 1], name_format, &lib_name[last_slash + 1]);

  return lib_name_fixed;
}


#if defined(_OSX_10_2_)
  /* NOTE this implementation is UNTESTED and therefore NOT SUPPORTED */
  #include <mach-o/dyld.h>
  /* According to http://developer.apple.com/library/mac/#documentation/DeveloperTools/Reference/MachOReference/Reference/reference.html

      The dynamic loader looks in the paths specified by a set of environment
      variables, and in the process' current directory, when it searches for a
      library. The environment variables are LD_LIBRARY_PATH, DYLD_LIBRARY_PATH,
      and DYLD_FALLBACK_LIBRARY_PATH. The default value of
      DYLD_FALLBACK_LIBRARY_PATH (used when this variable is not set),
      is $HOME/lib;/usr/local/lib;/usr/lib.
  */

  static SxcLibFunc* get_register_func(SxcContext* context, char* lib_name, int lib_name_len) {
    char* lib_name_fixed = fix_lib_name(context, lib_name, lib_name_len, "sxc_%s.dylib", '/');

    NSObjectFileImage file;
    NSModule module;
    NSSymbol symbol;
    NSObjectFileImageReturnCode return_code;
    NSLinkEditErrors error;
    int error_num;
    const char *error_file;
    const char *error_message;

    if(!_dyld_present()) {
      return sxc_error(context, "Could not load library %s (%s): dyld is not present", lib_name, lib_name_fixed);
    }

    return_code = NSCreateObjectFileImageFromFile(lib_name_fixed, &file);

    if (return_code == NSObjectFileImageSuccess) {
      /* NOTE not using the NSLINKMODULE_OPTION_PRIVATE flag in order to mimic
          the behavior on Windows and Unix */
      module = NSLinkModule(file, lib_name_fixed, NSLINKMODULE_OPTION_RETURN_ON_ERROR);
      NSDestroyObjectFileImage(file);

      if (module == NULL) {
        NSLinkEditError(&error, &error_num, &error_file, &error_message);
        return sxc_error(context, "Could not load library %s (%s): %s", lib_name, lib_name_fixed, error_message);
      }

      symbol = NSLookupSymbolInModule(module, SXC_LIB_REGISTER_FUNC);
      if (symbol == NULL) {
        return sxc_error(context, "Could not load library %s (%s): registration function not found", lib_name, lib_name_fixed);
      }

      /* SUCCESS! */
      return (SxcLibFunc*)NSAddressOfSymbol(symbol);

    } else {
      switch (return_code) {
        case NSObjectFileImageInappropriateFile:
          error_message = "file is not a bundle";
        case NSObjectFileImageArch:
          error_message = "library is for wrong CPU type";
        case NSObjectFileImageFormat:
          error_message = "bad format";
        case NSObjectFileImageAccess:
          error_message = "cannot access file";
        case NSObjectFileImageFailure:
        default:
          error_message = "unable to load library";
      }

      return sxc_error(context, "Could not load library %s (%s): %s", lib_name, lib_name_fixed, error_message);
    }
  }


#elif defined(_WINDOWS_)
  #include <windows.h>
  /* According to http://msdn.microsoft.com/en-us/library/ms684175%28v=vs.85%29.aspx

      The first directory searched is the directory containing the image file
      used to create the calling process. Doing this allows private dynamic-link
      library (DLL) files associated with a process to be found without adding
      the process's installed directory to the PATH environment variable. If a
      relative path is specified, the entire relative path is appended to every
      token in the DLL search path list.


     And according to http://msdn.microsoft.com/en-us/library/ms682586%28v=vs.85%29.aspx

      If SafeDllSearchMode is enabled, the search order is as follows:
        1. The directory from which the application loaded.
        2. The system directory.
        3. The 16-bit system directory.
        4. The Windows directory.
        5. The current directory.
        6. The directories that are listed in the PATH environment variable.
  */

  static SxcLibFunc* get_register_func(SxcContext* context, char* lib_name, int lib_name_len) {
    char* lib_name_fixed = fix_lib_name(context, lib_name, lib_name_len, "sxc_%s.dll", '\\');

    HINSTANCE library;
    void* func;
    int error;
    char error_message[256];

    library = LoadLibrary(lib_name_fixed);
    if (library != NULL) {
      func = GetProcAddress(library, SXC_LIB_REGISTER_FUNC);
      if (func != NULL) {
        /* SUCCESS! */
        return (SxcLibFunc*)func;
      }
    }

    error = GetLastError();
    if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
          NULL, error, 0, error_message, sizeof(error_message), NULL)) {
      return sxc_error(context, "Could not load library %s (%s): %s", lib_name, lib_name_fixed, error_message);
    } else {
      return sxc_error(context, "Could not load library %s (%s): system error %d", lib_name, lib_name_fixed, error);
    }
  }


#elif defined(_UNIX_)
  #include <dlfcn.h>
  /* According to http://www.qnx.com/developers/docs/6.4.0/neutrino/lib_ref/d/dlopen.html

      dlopen() searches the following, in order:
        * the runtime library search path that was set using the -rpath option
          to ld when the binary was linked
        * directories specified by the LD_LIBRARY_PATH environment variable
        * directories specified by the _CS_LIBPATH configuration string
  */

  static SxcLibFunc* get_register_func(SxcContext* context, char* lib_name, int lib_name_len) {
    char* lib_name_fixed = fix_lib_name(context, lib_name, lib_name_len, "libsxc_%s.so", '/');

    void* library;
    void* func;

    library = dlopen(lib_name_fixed, RTLD_NOW);
    if (library != NULL) {
      func = dlsym(library, SXC_LIB_REGISTER_FUNC);
      if (func != NULL) {
        /* SUCCESS! */
        return (SxcLibFunc*)func;
      }
    }

    return sxc_error(context, "Could not load library %s (%s): %s", lib_name, lib_name_fixed, dlerror());
  }


#else
  #error Could not determine target platform for dynamic library loading.  Define _UNIX_, _WINDOWS_, or _OSX_10_2_.
#endif



typedef struct _LoadedLib {
  char* name;
  SxcLibFunc* register_func;
  struct _LoadedLib* next;
} LoadedLib;

LoadedLib* loaded_libs;

void sxc_load(SxcContext* context) {
  char* lib_name;
  int lib_name_len;
  LoadedLib* lib;

printf("in sxc_load, lib_name:%p lib_name_len:%p\n", &lib_name, &lib_name_len);

  /* extract lib_name from args */
  if (sxc_arg(context, 0, sxc_cchars, &lib_name, &lib_name_len) == SXC_FAILURE) {
    sxc_error(context, "First argument to library loader must be a library name (string)");
    return;
  }

printf("done extract lib_name from args, name:%s, len:%d\n", lib_name, lib_name_len);

  /* find lib if it's already been loaded by this name */
  /* NOTE because of the dynamic library loaders' search paths, the same library
      can be referred to by multiple names.  However, each platform's loader
      does track the loaded binaries and doesn't try to load the same binary
      more than once. */
  /* TODO change this behavior! load only once */
  lib = loaded_libs;
  while (lib != NULL && strncmp(lib->name, lib_name, lib_name_len) != 0) ;

printf("done find lib if it's already been loaded\n");

  /* if library is new, load and invoke register function */
  if (lib == NULL) {
    /* space for name is allocated immediately following the struct */
    lib = malloc(sizeof(LoadedLib) + lib_name_len + 1);
printf("done alloc new lib struct, name_len: %d\n", lib_name_len);
    lib->name = (char*)(lib + 1);
    memcpy(lib->name, lib_name, lib_name_len);
printf("done set new lib name\n");
    lib->name[lib_name_len] = '\0';
printf("done terminate new lib name\n");

    lib->register_func = get_register_func(context, lib_name, lib_name_len);
printf("get new lib register_func\n");

    /* insert into list */
    lib->next = loaded_libs;
    loaded_libs = lib;

    (lib->register_func)(context);
printf("done register_func()\n");
  }
}
