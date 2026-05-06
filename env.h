#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <stddef.h>
#include <stdbool.h>

/* Forward declaration to avoid circular dependency with values.h */
typedef struct RuntimeVal RuntimeVal;

typedef struct {
  char *key;
  RuntimeVal *value;
} HashEntry;

typedef struct Environment Environment;

struct Environment {
  Environment *parent;
  HashEntry *entries;
  size_t capacity;
  size_t size;
  char *scope_name;
  int ref_count;
  void *owned_program; /* imported module AST owned by env */
  void **so_handles;   /* dlopen handles owned by env */
  size_t so_handle_count;
  size_t registry_index;
};

Environment *create_environment(Environment *parent, char *scope_name);
void retain_env(Environment *env);
void release_env(Environment *env);
void break_env_cycles(Environment *env);

void free_environment(Environment *env);

void declare_var(Environment *env, const char *varname, RuntimeVal *value);
void declare_owned(Environment *env, const char *varname, RuntimeVal *value);
void assign_var(Environment *env, const char *varname, RuntimeVal *value);
RuntimeVal *lookup_var(Environment *env, const char *varname);
Environment *resolve(Environment *env, const char *varname);

extern Environment *builtins_env;
Environment *get_builtins_env(void);

#endif
