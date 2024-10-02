#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stddef.h>
#include <stdbool.h>

#include "values.h"

typedef struct {
  char *key;
  RuntimeVal *value;
} HashEntry;

struct Environment {
  Environment *parent;
  HashEntry *entries;
  size_t capacity;
  size_t size;
  char *scope_name;
};

Environment *create_env();
Environment *create_environment(Environment *parent, char *scope_name);
void declare_var(Environment *env, const char *varname, RuntimeVal *value);
void assign_var(Environment *env, const char *varname, RuntimeVal *value);
RuntimeVal *lookup_var(Environment *env, const char *varname);
Environment *resolve(Environment *env, const char *varname);
void free_environment(Environment *env);

#endif  // ENVIRONMENT_H
