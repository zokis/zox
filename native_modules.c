/* Central native module registry. */

#include "native_modules.h"

#include "native_modules/math.c"
#include "native_modules/file.c"
#include "native_modules/string.c"
#include "native_modules/os.c"

NativeModule native_modules[] = {
  {"math",   init_math_module},
  {"file",   init_file_module},
  {"string", init_string_module},
  {"os",     init_os_module},
  {NULL, NULL}
};
