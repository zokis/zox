#include "env.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "hash.h"
#include "malloc_safe.h"

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR_THRESHOLD 0.75

Environment *create_environment(Environment *parent, char *scope_name) {
  Environment *env = (Environment *)malloc_safe(
      sizeof(Environment), "Failed to allocate memory for Environment");
  env->parent = parent;
  env->capacity = INITIAL_CAPACITY;
  env->size = 0;
  env->entries = (HashEntry *)calloc(env->capacity, sizeof(HashEntry));
  env->scope_name = scope_name;
  return env;
}

static void resize_hash_table(Environment *env) {
  size_t new_capacity = env->capacity * 2;
  HashEntry *new_entries = (HashEntry *)calloc(new_capacity, sizeof(HashEntry));

  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key != NULL) {
      size_t index = hash(env->entries[i].key, new_capacity);
      while (new_entries[index].key != NULL) {
        index = (index + 1) % new_capacity;
      }
      new_entries[index] = env->entries[i];
    }
  }

  free_safe(env->entries);
  env->entries = new_entries;
  env->capacity = new_capacity;
}

void declare_var(Environment *env, const char *varname, RuntimeVal *value) {
  if ((float)env->size / env->capacity >= LOAD_FACTOR_THRESHOLD) {
    resize_hash_table(env);
  }

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
  env->entries[index].key = strdup(varname);
  env->entries[index].value = value;
  env->size++;
}

void assign_var(Environment *env, const char *varname, RuntimeVal *value) {
  Environment *resolved_env = resolve(env, varname);
  size_t index = hash(varname, resolved_env->capacity);

  while (resolved_env->entries[index].key != NULL) {
    if (strcmp(resolved_env->entries[index].key, varname) == 0) {
      resolved_env->entries[index].value = value;
      return;
    }
    index = (index + 1) % resolved_env->capacity;
  }
}

RuntimeVal *lookup_var(Environment *env, const char *varname) {
  Environment *resolved_env = resolve(env, varname);
  size_t index = hash(varname, resolved_env->capacity);

  while (resolved_env->entries[index].key != NULL) {
    if (strcmp(resolved_env->entries[index].key, varname) == 0) {
      RuntimeVal *value = resolved_env->entries[index].value;
      return value;
    }
    index = (index + 1) % resolved_env->capacity;
  }
  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Value not found for variable %s\n", varname);
  error(error_message);
}

Environment *resolve(Environment *env, const char *varname) {
  Environment *current = env;

  while (current != NULL) {
    size_t index = hash(varname, current->capacity);

    while (current->entries[index].key != NULL) {
      if (strcmp(current->entries[index].key, varname) == 0) {
        return current;
      }
      index = (index + 1) % current->capacity;
    }

    current = current->parent;
  }
  char error_message[100];
  snprintf(error_message, sizeof(error_message),
           "Cannot resolve variable '%s' as it does not exist.", varname);
  error(error_message);
}

void free_environment(Environment *env) {
  for (size_t i = 0; i < env->capacity; i++) {
    if (env->entries[i].key != NULL) {
      free(env->entries[i].key);
    }
  }
  free(env->entries);
  free(env);
}
