// builtins.h
#ifndef BUILTINS_H
#define BUILTINS_H

#include "values.h"
#include "env.h"

void register_builtins(Environment *env);

RuntimeVal *builtin_keys(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_len(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_println_value(Environment *env, RuntimeVal **args, size_t arg_count);       
RuntimeVal *builtin_values(Environment *env, RuntimeVal **args, size_t arg_count);
RuntimeVal *builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count);

#endif // BUILTINS_H