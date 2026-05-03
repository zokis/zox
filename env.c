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
#include "zox_alloc.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR_THRESHOLD 0.75

static Environment *alloc_environment(void) {
  return (Environment *)zox_alloc_obj(ZOX_ALLOC_ENV, sizeof(Environment), "Environment");
}

static Environment **all_envs = NULL;
static size_t all_envs_count = 0;
static size_t all_envs_cap = 0;

static void break_val_env(RuntimeVal *val);
static void detach_val_env(RuntimeVal *val);
static void cleanup_env_entries(Environment *env);
static void cleanup_env_storage(Environment *env);

static void register_env(Environment *env) {
  if (all_envs_count >= all_envs_cap) {
    all_envs_cap = all_envs_cap ? all_envs_cap * 2 : 64;
    all_envs = (Environment **)realloc(all_envs, all_envs_cap * sizeof(Environment *));
  }
  all_envs[all_envs_count++] = env;
}

static void unregister_env(Environment *env) {
  for (size_t i = 0; i < all_envs_count; i++) {
    if (all_envs[i] == env) {
      all_envs[i] = all_envs[all_envs_count - 1];
      all_envs_count--;
      return;
    }
  }
}

Environment *create_environment(Environment *parent, char *scope_name) {
  Environment *env = alloc_environment();
  env->parent     = parent;
  env->capacity   = INITIAL_CAPACITY;
  env->size       = 0;
  env->entries    = (HashEntry *)calloc(env->capacity, sizeof(HashEntry));
  env->scope_name = scope_name ? strdup(scope_name) : NULL;
  env->ref_count  = 1;
  env->owned_program = NULL;
  env->so_handles      = NULL;
  env->so_handle_count = 0;
  if (parent) retain_env(parent);
  register_env(env);
  return env;
}

void retain_env(Environment *env) {
  if (env) env->ref_count++;
}

static void destroy_environment(Environment *env) {
  cleanup_env_storage(env);
  Environment *parent = env->parent;
  unregister_env(env);
  zox_free_obj(ZOX_ALLOC_ENV, env);
  if (parent) release_env(parent);
}

void release_env(Environment *env) {
  if (!env) return;
  env->ref_count--;
  if (env->ref_count <= 0) destroy_environment(env);
}

static void break_env_internal(Environment *env) {
  if (!env) return;
  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key == NULL) continue;
    detach_val_env(env->entries[i].value);
  }
}

static void cleanup_env_entries(Environment *env) {
  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key == NULL) continue;
    free_safe(env->entries[i].key);
    release(env->entries[i].value);
  }
  free_safe(env->entries);
}

static void cleanup_env_storage(Environment *env) {
  cleanup_env_entries(env);
  if (env->owned_program) {
    free_program((Program *)env->owned_program);
  }
  if (env->scope_name) {
    free_safe(env->scope_name);
  }
#ifndef _WIN32
  for (size_t i = 0; i < env->so_handle_count; i++) {
    dlclose(env->so_handles[i]);
  }
  free_safe(env->so_handles);
#endif
}

static void break_val_env(RuntimeVal *val) {
  if (!val) return;
  if (val->type == FUNCTION_T) {
    FunctionVal *fv = (FunctionVal *)val;
    if (fv->env) {
      Environment *captured = fv->env;
      fv->env = NULL;
      release_env(captured);
    }
  } else if (val->type == LIST_T) {
    ListVal *lv = (ListVal *)val;
    for (size_t i = 0; i < lv->size; i++)
      break_val_env(lv->items[i]);
  } else if (val->type == DICT_T) {
    DictVal *dv = (DictVal *)val;
    for (size_t i = 0; i < dv->capacity; i++)
      if (dv->entries[i].key != NULL)
        break_val_env(dv->entries[i].value);
  }
}

static void detach_val_env(RuntimeVal *val) {
  if (!val) return;
  if (val->type == FUNCTION_T) {
    ((FunctionVal *)val)->env = NULL;
  } else if (val->type == LIST_T) {
    ListVal *lv = (ListVal *)val;
    for (size_t i = 0; i < lv->size; i++) {
      detach_val_env(lv->items[i]);
    }
  } else if (val->type == DICT_T) {
    DictVal *dv = (DictVal *)val;
    for (size_t i = 0; i < dv->capacity; i++) {
      if (dv->entries[i].key != NULL) {
        detach_val_env(dv->entries[i].value);
      }
    }
  }
}

void break_env_cycles(Environment *env) {
  if (all_envs_count == 0) return;

  /* Shutdown Pass 1: Break all closure links. 
     We don't release_env here to avoid recursive destruction. */
  for (size_t i = 0; i < all_envs_count; i++) {
    break_env_internal(all_envs[i]);
  }

  /* Shutdown Pass 2: Manually free all environments in the registry.
     We iterate backwards and free everything. unregister_env will handle 
     the removal from the array. */
  while (all_envs_count > 0) {
    Environment *e = all_envs[all_envs_count - 1];

    cleanup_env_storage(e);

    /* Important: We don't release_env(parent) here as the parent 
       is already in all_envs and will be freed by this loop. */
    all_envs_count--; 
    zox_free_obj(ZOX_ALLOC_ENV, e);
  }

  if (all_envs) {
    free(all_envs);
    all_envs = NULL;
    all_envs_cap = 0;
  }
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
