/* zox_module.h — API pública para módulos externos (.so)
   Inclua este header ao escrever um módulo dinâmico para Zox.
   Compile com: gcc -shared -fPIC -o modulo.so modulo.c

   O módulo deve exportar exatamente uma função:
       void zox_init_module(Environment *env);
   que registra as funções do módulo no env recebido. */

#ifndef ZOX_MODULE_H
#define ZOX_MODULE_H

#include "values.h"
#include "env.h"
#include "hash.h"
#include "malloc_safe.h"

/* Funções do runtime necessárias para módulos externos */
void        list_append_val(ListVal *list, RuntimeVal *item);
void        dict_set_val(DictVal *dict, const char *key, RuntimeVal *value);
RuntimeVal *zox_call_function(FunctionVal *func, Environment *env, RuntimeVal **args, size_t arg_count);

/* Ponto de entrada obrigatório de todo módulo externo */
typedef void (*ZoxModuleInitFn)(Environment *env);
#define ZOX_MODULE_INIT void zox_init_module(Environment *env)

#endif /* ZOX_MODULE_H */
