#ifndef BUILTINS_H
#define BUILTINS_H

#include "env.h"
#include "values.h"
#include <stddef.h>

void register_builtins(Environment *env);

RuntimeVal *builtin_keys(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_has_key(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_get(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_len(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_println_value(Environment *env, RuntimeVal **args,
                                  size_t arg_count);
RuntimeVal *builtin_values(Environment *env, RuntimeVal **args,
                           size_t arg_count);
RuntimeVal *builtin_print_value(Environment *env, RuntimeVal **args,
                                size_t arg_count);
RuntimeVal *builtin_sum(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_typeof(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_ok(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_err(Environment *env, RuntimeVal **args, size_t arg_count);

#endif // BUILTINS_H
