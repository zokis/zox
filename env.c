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
#include "values.h"
#include "zox_alloc.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR_THRESHOLD 0.75

static Environment *alloc_environment(void) {
  return (Environment *)zox_alloc_obj(ZOX_ALLOC_ENV, sizeof(Environment), "Environment");
}

Environment *builtins_env = NULL;

Environment *get_builtins_env(void) {
  return builtins_env;
}

static Environment **all_envs = NULL;
static size_t all_envs_count = 0;
static size_t all_envs_cap = 0;

static void detach_val_env(RuntimeVal *val);
static int find_entry_index(Environment *env, const char *varname, size_t *index_out);

static void cleanup_full_env(Environment *env) {
  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key != NULL) {
      zox_free_buf(ZOX_BUF_STRING, env->entries[i].key);
      release(env->entries[i].value);
    }
  }
  if (env->entries) {
    zox_free_buf(ZOX_BUF_ENV_ENTRIES, env->entries);
    env->entries = NULL;
  }
  
  if (env->owned_program) {
    free_program((Program *)env->owned_program);
    env->owned_program = NULL;
  }
  if (env->scope_name) {
    zox_free_buf(ZOX_BUF_SCOPE_NAME, env->scope_name);
    env->scope_name = NULL;
  }
#ifndef _WIN32
  for (size_t i = 0; i < env->so_handle_count; i++) {
    dlclose(env->so_handles[i]);
  }
  if (env->so_handles) {
    zox_free_buf(ZOX_BUF_MISC, env->so_handles);
    env->so_handles = NULL;
  }
  env->so_handle_count = 0;
#endif
}

static void register_env(Environment *env) {
  if (all_envs_count >= all_envs_cap) {
    all_envs_cap = all_envs_cap ? all_envs_cap * 2 : 64;
    all_envs = (Environment **)zox_realloc_buf(
        ZOX_BUF_MISC, all_envs, all_envs_cap * sizeof(Environment *), "env registry");
  }
  env->registry_index = all_envs_count;
  all_envs[all_envs_count++] = env;
}

static void unregister_env(Environment *env) {
  size_t idx = env->registry_index;
  if (idx < all_envs_count && all_envs[idx] == env) {
    Environment *last = all_envs[all_envs_count - 1];
    all_envs[idx] = last;
    last->registry_index = idx;
    all_envs_count--;
  }
}

static void env_pool_cleanup(void *ptr) {
  Environment *env = (Environment *)ptr;
  if (env->entries) {
    zox_free_buf(ZOX_BUF_ENV_ENTRIES, env->entries);
    env->entries = NULL;
  }
}

Environment *create_environment(Environment *parent, char *scope_name) {
  static int cleanup_registered = 0;
  if (!cleanup_registered) {
    zox_alloc_set_cleanup_fn(ZOX_ALLOC_ENV, env_pool_cleanup);
    cleanup_registered = 1;
  }

  Environment *env = alloc_environment();
  env->parent      = parent;

  if (!env->entries) {
    env->capacity = INITIAL_CAPACITY;
    env->entries  = (HashEntry *)zox_calloc_buf(
        ZOX_BUF_ENV_ENTRIES, env->capacity, sizeof(HashEntry), "env entries");
  } else {
    env->size = 0;
    memset(env->entries, 0, env->capacity * sizeof(HashEntry));
  }
  
  env->scope_name = scope_name ? zox_strdup_buf(ZOX_BUF_SCOPE_NAME, scope_name) : NULL;
  env->ref_count  = 1;
  env->owned_program = NULL;
  env->so_handles      = NULL;
  env->so_handle_count = 0;
  env->registry_index  = 0;
  
  if (parent) retain_env(parent);
  register_env(env);
  return env;
}

void retain_env(Environment *env) {
  if (env) env->ref_count++;
}

static void destroy_environment(Environment *env) {
  /* Step 1: Unregister BEFORE any cleanup or pooling. */
  unregister_env(env);

  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key != NULL) {
      zox_free_buf(ZOX_BUF_STRING, env->entries[i].key);
      release(env->entries[i].value);
      env->entries[i].key = NULL;
      env->entries[i].value = NULL;
    }
  }
  
  if (env->owned_program) {
    free_program((Program *)env->owned_program);
    env->owned_program = NULL;
  }
  if (env->scope_name) {
    zox_free_buf(ZOX_BUF_SCOPE_NAME, env->scope_name);
    env->scope_name = NULL;
  }
#ifndef _WIN32
  for (size_t i = 0; i < env->so_handle_count; i++) {
    dlclose(env->so_handles[i]);
  }
  if (env->so_handles) {
    zox_free_buf(ZOX_BUF_MISC, env->so_handles);
    env->so_handles = NULL;
  }
  env->so_handle_count = 0;
#endif

  Environment *parent = env->parent;
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

static int find_entry_index(Environment *env, const char *varname, size_t *index_out) {
  size_t index = hash(varname, env->capacity);

  while (env->entries[index].key != NULL) {
    if (strcmp(env->entries[index].key, varname) == 0) {
      *index_out = index;
      return 1;
    }
    index = (index + 1) % env->capacity;
  }

  *index_out = index;
  return 0;
}

void break_env_cycles(Environment *env) {
  (void)env;
  if (all_envs_count == 0) return;

  for (size_t i = 0; i < all_envs_count; i++) {
    break_env_internal(all_envs[i]);
  }

  while (all_envs_count > 0) {
    Environment *e = all_envs[all_envs_count - 1];
    cleanup_full_env(e);
    all_envs_count--; 
    zox_free_obj(ZOX_ALLOC_ENV, e);
  }

  if (all_envs) {
    zox_free_buf(ZOX_BUF_MISC, all_envs);
    all_envs = NULL;
    all_envs_cap = 0;
  }
}

void free_environment(Environment *env) {
  release_env(env);
}

static void resize_hash_table(Environment *env) {
  size_t old_capacity = env->capacity;
  size_t new_capacity = old_capacity * 2;
  HashEntry *new_entries = (HashEntry *)zox_calloc_buf(
      ZOX_BUF_ENV_ENTRIES, new_capacity, sizeof(HashEntry), "resize_hash_table");

  for (size_t i = 0; i < old_capacity; i++) {
    if (env->entries[i].key != NULL) {
      size_t index = hash(env->entries[i].key, new_capacity);
      while (new_entries[index].key != NULL)
        index = (index + 1) % new_capacity;
      new_entries[index] = env->entries[i];
    }
  }

  zox_free_buf(ZOX_BUF_ENV_ENTRIES, env->entries);
  env->entries  = new_entries;
  env->capacity = new_capacity;
}

void declare_var(Environment *env, const char *varname, RuntimeVal *value) {
  size_t index;

  if ((float)env->size / env->capacity >= LOAD_FACTOR_THRESHOLD)
    resize_hash_table(env);

  if (find_entry_index(env, varname, &index)) {
    char error_message[100];
    snprintf(error_message, sizeof(error_message),
             "Cannot declare variable %s. It is already defined.\n", varname);
    error(error_message);
  }

  env->entries[index].key   = zox_strdup_buf(ZOX_BUF_STRING, varname);
  env->entries[index].value = value;
  retain(value);
  env->size++;
}

void declare_owned(Environment *env, const char *varname, RuntimeVal *value) {
  declare_var(env, varname, value);
  release(value);
}

void assign_var(Environment *env, const char *varname, RuntimeVal *value) {
  Environment *current = env;
  size_t index;

  while (current != NULL) {
    if (find_entry_index(current, varname, &index)) {
      if (current->entries[index].value == value) return;
      release(current->entries[index].value);
      current->entries[index].value = value;
      retain(value);
      return;
    }
    current = current->parent;
  }

  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Cannot assign variable '%s' as it does not exist.", varname);
  error(error_message);
}

RuntimeVal *lookup_var(Environment *env, const char *varname) {
  Environment *current = env;
  size_t index;

  while (current != NULL) {
    if (find_entry_index(current, varname, &index)) {
      return current->entries[index].value;
    }
    current = current->parent;
  }

  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Value not found for variable %s\n", varname);
  error(error_message);
  return NULL;
}

Environment *resolve(Environment *env, const char *varname) {
  Environment *current = env;
  size_t index;

  while (current != NULL) {
    if (find_entry_index(current, varname, &index)) {
      return current;
    }
    current = current->parent;
  }
  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Cannot resolve variable '%s' as it does not exist.", varname);
  error(error_message);
  return NULL;
}
