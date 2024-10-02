#ifndef NATIVE_MODULES_H
#define NATIVE_MODULES_H

#include "ast.h"
#include "values.h"

typedef struct {
  const char *name;
  void (*init_func)(Environment *env);
} NativeModule;

static RuntimeVal *math_abs(Environment *env, RuntimeVal **args, int arg_count);
static RuntimeVal *math_sqrt(Environment *env, RuntimeVal **args,
                             int arg_count);
void init_math_module(Environment *env);
void init_file_module(Environment *env);

NativeModule native_modules[] = {
    {"math", init_math_module}, {"file", init_file_module}, {0, 0}};

#endif