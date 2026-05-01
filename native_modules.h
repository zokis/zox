#ifndef NATIVE_MODULES_H
#define NATIVE_MODULES_H

#include "ast.h"
#include "values.h"

typedef struct {
  const char *name;
  void (*init_func)(Environment *env);
} NativeModule;

void init_math_module(Environment *env);
void init_file_module(Environment *env);
void init_string_module(Environment *env);
void init_os_module(Environment *env);

extern NativeModule native_modules[];

#endif
