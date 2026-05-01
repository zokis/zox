#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "ast.h"
#include "env.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "hash.h"
#include "malloc_safe.h"
#include "values.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR_THRESHOLD 0.75

Environment *create_environment(Environment *parent, char *scope_name) {
  Environment *env = (Environment *)malloc_safe(
      sizeof(Environment), "Failed to allocate memory for Environment");
  env->parent     = parent;
  env->capacity   = INITIAL_CAPACITY;
  env->size       = 0;
  env->entries    = (HashEntry *)calloc(env->capacity, sizeof(HashEntry));
  env->scope_name = scope_name;
  env->ref_count  = 1;
  env->owned_program = NULL;
  env->so_handles      = NULL;
  env->so_handle_count = 0;
  if (parent) retain_env(parent);
  return env;
}

void retain_env(Environment *env) {
  if (env) env->ref_count++;
}

static void destroy_environment(Environment *env) {
  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key != NULL) {
      free_safe(env->entries[i].key);
      release(env->entries[i].value);
    }
  }
  free_safe(env->entries);
  if (env->owned_program) {
    free_program((Program *)env->owned_program);
  }
#ifndef _WIN32
  for (size_t _i = 0; _i < env->so_handle_count; _i++)
    dlclose(env->so_handles[_i]);
  if (env->so_handles) free(env->so_handles);
#endif
  Environment *parent = env->parent;
  free_safe(env);
  if (parent) release_env(parent);
}

void release_env(Environment *env) {
  if (!env) return;
  env->ref_count--;
  if (env->ref_count <= 0) destroy_environment(env);
}

/* Break captured-env reference cycles before final environment release.

   Imported functions capture module_env. That env can also contain private
   functions capturing same module_env, forming cycles not visible from global.

   Visited envs prevent infinite loops in cyclic graphs. */
static Environment **visited_envs  = NULL;
static size_t        visited_count = 0;
static size_t        visited_cap   = 0;

static int already_visited(Environment *env) {
  for (size_t i = 0; i < visited_count; i++)
    if (visited_envs[i] == env) return 1;
  return 0;
}

static void mark_visited(Environment *env) {
  if (visited_count >= visited_cap) {
    visited_cap = visited_cap ? visited_cap * 2 : 16;
    visited_envs = (Environment **)realloc(visited_envs,
                                           visited_cap * sizeof(Environment *));
  }
  visited_envs[visited_count++] = env;
}

static void break_val_env(RuntimeVal *val);

static void break_env_all(Environment *env) {
  if (!env) return;
  if (already_visited(env)) return;
  mark_visited(env);
  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key == NULL) continue;
    break_val_env(env->entries[i].value);
  }
}

static void break_val_env(RuntimeVal *val) {
  if (!val) return;
  if (val->type == FUNCTION_T) {
    FunctionVal *fv = (FunctionVal *)val;
    if (fv->env) {
      Environment *captured = fv->env;
      fv->env = NULL;
      /* captured env first -> break internal module cycles */
      break_env_all(captured);
      release_env(captured);
    }
  } else if (val->type == LIST_T) {
    ListVal *lv = (ListVal *)val;
    for (size_t i = 0; i < lv->size; i++)
      break_val_env(lv->items[i]);
  } else if (val->type == DICT_T) {
    DictVal *dv = (DictVal *)val;
    for (size_t i = 0; i < dv->capacity; i++)
      for (Entry *e = dv->entries[i]; e != NULL; e = e->next)
        break_val_env(e->value);
  }
}

void break_env_cycles(Environment *env) {
  visited_count = 0;
  break_env_all(env);
  free(visited_envs);
  visited_envs  = NULL;
  visited_cap   = 0;
  visited_count = 0;
}

void free_environment(Environment *env) {
  release_env(env);
}

static void resize_hash_table(Environment *env) {
  size_t new_capacity = env->capacity * 2;
  HashEntry *new_entries = (HashEntry *)calloc(new_capacity, sizeof(HashEntry));

  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key != NULL) {
      size_t index = hash(env->entries[i].key, new_capacity);
      while (new_entries[index].key != NULL)
        index = (index + 1) % new_capacity;
      new_entries[index] = env->entries[i];
    }
  }

  free_safe(env->entries);
  env->entries  = new_entries;
  env->capacity = new_capacity;
}

void declare_var(Environment *env, const char *varname, RuntimeVal *value) {
  if ((float)env->size / env->capacity >= LOAD_FACTOR_THRESHOLD)
    resize_hash_table(env);

  size_t index = hash(varname, env->capacity);
  while (env->entries[index].key != NULL) {
    if (strcmp(env->entries[index].key, varname) == 0) {
      char error_message[100];
      snprintf(error_message, sizeof(error_message),
               "Cannot declare variable %s. It is already defined.\n", varname);
      error(error_message);
    }
    index = (index + 1) % env->capacity;
  }
  env->entries[index].key   = strdup(varname);
  env->entries[index].value = value;
  retain(value);
  env->size++;
}

void declare_owned(Environment *env, const char *varname, RuntimeVal *value) {
  declare_var(env, varname, value);
  release(value);
}

void assign_var(Environment *env, const char *varname, RuntimeVal *value) {
  Environment *resolved_env = resolve(env, varname);
  size_t index = hash(varname, resolved_env->capacity);

  while (resolved_env->entries[index].key != NULL) {
    if (strcmp(resolved_env->entries[index].key, varname) == 0) {
      if (resolved_env->entries[index].value == value) return; /* self-assign */
      release(resolved_env->entries[index].value);
      resolved_env->entries[index].value = value;
      retain(value);
      return;
    }
    index = (index + 1) % resolved_env->capacity;
  }
}

RuntimeVal *lookup_var(Environment *env, const char *varname) {
  Environment *resolved_env = resolve(env, varname);
  size_t index = hash(varname, resolved_env->capacity);

  while (resolved_env->entries[index].key != NULL) {
    if (strcmp(resolved_env->entries[index].key, varname) == 0)
      return resolved_env->entries[index].value;
    index = (index + 1) % resolved_env->capacity;
  }
  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Value not found for variable %s\n", varname);
  error(error_message);
  return NULL;
}

Environment *resolve(Environment *env, const char *varname) {
  Environment *current = env;
  while (current != NULL) {
    size_t index = hash(varname, current->capacity);
    while (current->entries[index].key != NULL) {
      if (strcmp(current->entries[index].key, varname) == 0)
        return current;
      index = (index + 1) % current->capacity;
    }
    current = current->parent;
  }
  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Cannot resolve variable '%s' as it does not exist.", varname);
  error(error_message);
  return NULL;
}
