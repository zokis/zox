/* Module imports: native, .zo, .so, .dll. */
#include "eval_internal.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

char *find_module_path(const char *module_name) {
  for (int i = 0; native_modules[i].name != NULL; i++) {
    if (strcmp(native_modules[i].name, module_name) == 0)
      return strdup("native");
  }

  size_t len = strlen(module_name);
  if ((len > 3 && strcmp(module_name + len - 3, ".so") == 0) ||
      (len > 4 && strcmp(module_name + len - 4, ".dll") == 0) ||
      (len > 3 && strcmp(module_name + len - 3, ".zo") == 0)) {
    if (access(module_name, F_OK) != -1)
      return strdup(module_name);
  }

#ifdef _WIN32
  char *paths[] = {".", ".\\lib", ".\\packages", "C:\\Program Files\\Zox\\packages"};
#else
  char *paths[] = {".", "./lib", "./packages", "/usr/local/lib/zox/packages"};
#endif
  char full_path[512];
  char module_path[256];
  strncpy(module_path, module_name, sizeof(module_path) - 1);
  module_path[sizeof(module_path) - 1] = '\0';
  char *p = module_path;
  while (*p) { if (*p == '.') *p = PATH_SEPARATOR[0]; p++; }

  for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); i++) {
    snprintf(full_path, sizeof(full_path), "%s%s%s.zo",
             paths[i], PATH_SEPARATOR, module_path);
    if (access(full_path, F_OK) != -1) return strdup(full_path);
#ifndef _WIN32
    snprintf(full_path, sizeof(full_path), "%s%s%s.so",
             paths[i], PATH_SEPARATOR, module_path);
    if (access(full_path, F_OK) != -1) return strdup(full_path);
#else
    snprintf(full_path, sizeof(full_path), "%s%s%s.dll",
             paths[i], PATH_SEPARATOR, module_path);
    if (access(full_path, F_OK) != -1) return strdup(full_path);
#endif
  }
  return NULL;
}

static RuntimeVal *eval_import_dynamic(const char *so_path,
                                       ImportStmt *import_stmt,
                                       Environment *env) {
#ifdef _WIN32
  char error_msg[256];
  snprintf(error_msg, sizeof(error_msg),
           "Dynamic modules (.dll) not yet supported on Windows: %s", so_path);
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

  Environment *module_env = create_environment(env, strdup(so_path));
  init_fn(module_env);

  for (size_t i = 0; i < import_stmt->import_count; i++) {
    ImportItem *item = import_stmt->imports[i];
    RuntimeVal *val  = lookup_var(module_env, item->name);
    if (!val) {
      char error_msg[256];
      snprintf(error_msg, sizeof(error_msg),
               "Cannot find '%s' in module '%s'", item->name, so_path);
      free_environment(module_env);
      dlclose(handle);
      error(error_msg);
    }
    declare_var(env, item->alias ? item->alias : item->name, val);
  }

  /* parent env owns dlopen handle */
  env->so_handles = realloc(env->so_handles,
                            sizeof(void *) * (env->so_handle_count + 1));
  env->so_handles[env->so_handle_count++] = handle;
  free_environment(module_env);
  return (RuntimeVal *)MK_NIL();
#endif
}

static int is_dynamic_lib(const char *path) {
  size_t len = strlen(path);
  if (len > 3 && strcmp(path + len - 3, ".so")  == 0) return 1;
  if (len > 4 && strcmp(path + len - 4, ".dll") == 0) return 1;
  return 0;
}

RuntimeVal *eval_import_stmt(ImportStmt *import_stmt, Environment *env) {
  char *module_path = find_module_path(import_stmt->module_name);
  if (!module_path) {
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg),
             "Module '%s' not found", import_stmt->module_name);
    error(error_msg);
  }

  if (strcmp(module_path, "native") == 0) {
    for (int i = 0; native_modules[i].name != NULL; i++) {
      if (strcmp(native_modules[i].name, import_stmt->module_name) == 0) {
        Environment *module_env = create_environment(env, import_stmt->module_name);
        native_modules[i].init_func(module_env);
        for (size_t j = 0; j < import_stmt->import_count; j++) {
          ImportItem *item = import_stmt->imports[j];
          RuntimeVal *val  = lookup_var(module_env, item->name);
          if (!val) {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg),
                     "Cannot find '%s' in module '%s'",
                     item->name, import_stmt->module_name);
            error(error_msg);
          }
          declare_var(env, item->alias ? item->alias : item->name, val);
        }
        free_environment(module_env);
        free_safe(module_path);
        return (RuntimeVal *)MK_NIL();
      }
    }
  }

  if (is_dynamic_lib(module_path)) {
    RuntimeVal *result = eval_import_dynamic(module_path, import_stmt, env);
    free_safe(module_path);
    return result;
  }

  char *module_code = read_file(module_path);
  Environment *module_env = create_environment(env, import_stmt->module_name);
  size_t token_count;
  Token  *tokens = tokenize(module_code, &token_count);
  Parser *parser = create_parser(tokens, token_count);
  Program *program = produce_ast(parser, module_code);
  RuntimeVal *prog_result = eval_program(program, module_env);
  release(prog_result);

  for (size_t i = 0; i < import_stmt->import_count; i++) {
    ImportItem *item = import_stmt->imports[i];
    RuntimeVal *val  = lookup_var(module_env, item->name);
    if (!val) {
      char error_msg[256];
      snprintf(error_msg, sizeof(error_msg),
               "Cannot find '%s' in module '%s'",
               item->name, import_stmt->module_name);
      error(error_msg);
    }
    declare_var(env, item->alias ? item->alias : item->name, val);
  }

  free_safe(module_code);
  free_tokens(tokens, token_count);
  free_safe(parser);
  module_env->owned_program = program;
  free_environment(module_env);
  free_safe(module_path);
  return (RuntimeVal *)MK_NIL();
}
