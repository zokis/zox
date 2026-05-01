/* Public API for external Zox modules (.so).
   Include this header when writing dynamic modules.
   Compile: gcc -shared -fPIC -o module.so module.c

   Module must export exactly one function:
       void zox_init_module(Environment *env);
   It registers module functions in given env. */

#ifndef ZOX_MODULE_H
#define ZOX_MODULE_H

#include "values.h"
#include "env.h"
#include "hash.h"
#include "malloc_safe.h"

/* Runtime hooks required by external modules. */
void        list_append_val(ListVal *list, RuntimeVal *item);
void        dict_set_val(DictVal *dict, const char *key, RuntimeVal *value);
RuntimeVal *zox_call_function(FunctionVal *func, Environment *env, RuntimeVal **args, size_t arg_count);

/* Required external module entry point. */
typedef void (*ZoxModuleInitFn)(Environment *env);
#define ZOX_MODULE_INIT void zox_init_module(Environment *env)

#endif /* ZOX_MODULE_H */
