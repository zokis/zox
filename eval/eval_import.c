#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "eval_internal.h"
#include "../lexer.h"
#include "../parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define MAX_PATH_LEN 1024

/* Declarations for items usually in main.c */
void run_program(Program *program, Environment *env);

static char *search_paths[] = {".", "lib", "/usr/local/lib/zox", NULL};

static int file_exists(const char *path) {
  struct stat buffer;
  return (stat(path, &buffer) == 0);
}

static char *dup_existing_path(const char *path) {
  if (file_exists(path)) {
    return strdup(path);
  }
  return NULL;
}

static void normalize_module_path(char *dest, size_t dest_size,
                                  const char *module_name) {
  snprintf(dest, dest_size, "%s", module_name);
  for (size_t i = 0; i < strlen(dest); i++) {
    if (dest[i] == '.') {
      dest[i] = PATH_SEP;
    }
  }
}

static char *find_in_search_paths(const char *module_path) {
  char full_path[MAX_PATH_LEN];
  const char *extensions[] = {".zo", ".so", ".dll", NULL};

  for (int i = 0; search_paths[i] != NULL; i++) {
    for (int j = 0; extensions[j] != NULL; j++) {
      snprintf(full_path, sizeof(full_path), "%s%c%s%s", search_paths[i],
               PATH_SEP, module_path, extensions[j]);
      if (file_exists(full_path)) {
        return strdup(full_path);
      }
    }
  }
  return NULL;
}

static char *resolve_module_path(const char *module_name) {
  char *found_path = NULL;
  char module_path[MAX_PATH_LEN];

  if (module_name[0] == '.' || module_name[0] == PATH_SEP) {
    found_path = dup_existing_path(module_name);
  } else {
    normalize_module_path(module_path, sizeof(module_path), module_name);
    found_path = find_in_search_paths(module_path);
  }

  return found_path;
}

static void declare_import_item(Environment *target_env, Environment *module_env,
                                ImportItem *item, const char *module_name) {
  RuntimeVal *val = lookup_var(module_env, item->name);
  if (!val) {
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg),
             "Cannot find '%s' in module '%s'", item->name, module_name);
    error(error_msg);
  }
  declare_var(target_env, item->alias ? item->alias : item->name, val);
}

static void declare_import_items(Environment *target_env, Environment *module_env,
                                 ImportStmt *import_stmt, const char *module_name) {
  for (size_t i = 0; i < import_stmt->import_count; i++) {
    declare_import_item(target_env, module_env, import_stmt->imports[i], module_name);
  }
}

static RuntimeVal *eval_import_native(ImportStmt *import_stmt, Environment *env) {
  for (int i = 0; native_modules[i].name != NULL; i++) {
    if (strcmp(native_modules[i].name, import_stmt->module_name) == 0) {
      Environment *module_env = create_environment(builtins_env, import_stmt->module_name);
      native_modules[i].init_func(module_env);
      
      if (import_stmt->import_count == 0) {
        ModuleVal *module = MK_MODULE(module_env);
        free_environment(module_env);
        return (RuntimeVal *)module;
      } else {
        declare_import_items(env, module_env, import_stmt, import_stmt->module_name);
        free_environment(module_env);
        return (RuntimeVal *)MK_NIL();
      }
    }
  }
  return NULL;
}

static RuntimeVal *eval_import_dynamic(const char *so_path,
                                       ImportStmt *import_stmt,
                                       Environment *env) {
#ifdef _WIN32
  char error_msg[512];
  snprintf(error_msg, sizeof(error_msg),
           "Dynamic module loading is not supported on Windows yet: '%s'", so_path);
  error(error_msg);
  return (RuntimeVal *)MK_NIL();
#else
  void *handle = dlopen(so_path, RTLD_LAZY);
  if (!handle) {
    char error_msg[512];
    snprintf(error_msg, sizeof(error_msg),
             "Cannot load dynamic module '%s': %s", so_path, dlerror());
    error(error_msg);
  }

  typedef void (*InitFn)(Environment *);
  void  *sym     = dlsym(handle, "zox_init_module");
  InitFn init_fn;
  memcpy(&init_fn, &sym, sizeof(init_fn));
  if (!init_fn) {
    char error_msg[512];
    snprintf(error_msg, sizeof(error_msg),
             "Module '%s' does not export 'zox_init_module'", so_path);
    dlclose(handle);
    error(error_msg);
  }

  Environment *module_env = create_environment(builtins_env, (char *)so_path);
  init_fn(module_env);

  RuntimeVal *result = (RuntimeVal *)MK_NIL();

  if (import_stmt->import_count == 0) {
    result = (RuntimeVal *)MK_MODULE(module_env);
  } else {
    declare_import_items(env, module_env, import_stmt, so_path);
  }

  /* parent env owns dlopen handle */
  env->so_handles = realloc_safe(env->so_handles,
                                 sizeof(void *) * (env->so_handle_count + 1),
                                 "so_handles");
  env->so_handles[env->so_handle_count++] = handle;
  free_environment(module_env);
  return result;
#endif
}

static int is_dynamic_lib(const char *path) {
  size_t len = strlen(path);
  if (len > 3 && strcmp(path + len - 3, ".so")  == 0) return 1;
  if (len > 4 && strcmp(path + len - 4, ".dll") == 0) return 1;
  return 0;
}

static int is_zox_file(const char *path) {
  size_t len = strlen(path);
  if (len > 3 && strcmp(path + len - 3, ".zo") == 0) return 1;
  return 0;
}

RuntimeVal *eval_import_stmt(ImportStmt *import_stmt, Environment *env) {
  RuntimeVal *res = eval_import_native(import_stmt, env);
  if (res) return res;

  char *full_path = resolve_module_path(import_stmt->module_name);
  if (full_path == NULL) {
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Module not found: '%s'",
             import_stmt->module_name);
    error(error_msg);
  }

  if (is_dynamic_lib(full_path)) {
    res = eval_import_dynamic(full_path, import_stmt, env);
    free(full_path);
    return res;
  }

  if (is_zox_file(full_path)) {
    FILE *file = fopen(full_path, "r");
    if (file == NULL) error("Cannot open module file.");
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *source = malloc_safe(length + 1, "module source");
    size_t read_bytes = fread(source, 1, length, file);
    if (read_bytes != (size_t)length) {
      fclose(file);
      free_safe(source);
      error("Failed to read module file content.");
    }
    source[length] = '\0';
    fclose(file);

    size_t token_count;
    Token *tokens = tokenize(source, &token_count);
    Parser *parser = create_parser(tokens, token_count);
    Program *program = produce_ast(parser, source);
    
    free(source);
    if (parser) free_safe(parser);
    if (tokens) free_tokens(tokens, (int)token_count);

    Environment *module_env = create_environment(builtins_env, import_stmt->module_name);
    module_env->owned_program = program;
    run_program(program, module_env);

    RuntimeVal *result = (RuntimeVal *)MK_NIL();
    if (import_stmt->import_count == 0) {
      result = (RuntimeVal *)MK_MODULE(module_env);
    } else {
      declare_import_items(env, module_env, import_stmt, import_stmt->module_name);
    }

    free_environment(module_env);
    free(full_path);
    return result;
  }

  free(full_path);
  return (RuntimeVal *)MK_NIL();
}
