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
  int ref_count;
  void *owned_program; /* AST do modulo importado — liberada junto com o env */
  void **so_handles;   /* array de handles dlopen() — fechados junto com o env */
  size_t so_handle_count;
};

Environment *create_environment(Environment *parent, char *scope_name);
void retain_env(Environment *env);
void release_env(Environment *env);
void free_environment(Environment *env);
void break_env_cycles(Environment *env);
void declare_var(Environment *env, const char *varname, RuntimeVal *value);
void declare_owned(Environment *env, const char *varname, RuntimeVal *value);
void assign_var(Environment *env, const char *varname, RuntimeVal *value);
RuntimeVal *lookup_var(Environment *env, const char *varname);
Environment *resolve(Environment *env, const char *varname);

#endif
