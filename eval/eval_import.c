/* Module imports: native, .zo, .so, .dll. */
#include "eval_internal.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

static int is_native_module(const char *module_name) {
  for (int i = 0; native_modules[i].name != NULL; i++) {
    if (strcmp(native_modules[i].name, module_name) == 0) {
      return 1;
    }
  }
  return 0;
}

static int has_module_extension(const char *module_name) {
  size_t len = strlen(module_name);
  return (len > 3 && strcmp(module_name + len - 3, ".so") == 0) ||
         (len > 4 && strcmp(module_name + len - 4, ".dll") == 0) ||
         (len > 3 && strcmp(module_name + len - 3, ".zo") == 0);
}

static void normalize_module_path(char *module_path, size_t size,
                                  const char *module_name) {
  strncpy(module_path, module_name, size - 1);
  module_path[size - 1] = '\0';
  for (char *p = module_path; *p; p++) {
    if (*p == '.') {
      *p = PATH_SEPARATOR[0];
    }
  }
}

static char *dup_existing_path(const char *path) {
  if (access(path, F_OK) == -1) {
    return NULL;
  }
  return strdup(path);
}

static char *find_in_search_paths(const char *module_path) {
#ifdef _WIN32
  char *paths[] = {".", ".\\lib", ".\\packages", "C:\\Program Files\\Zox\\packages"};
#else
  char *paths[] = {".", "./lib", "./packages", "/usr/local/lib/zox/packages"};
#endif
  char full_path[512];
  char *found_path = NULL;

  for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); i++) {
    snprintf(full_path, sizeof(full_path), "%s%s%s.zo",
             paths[i], PATH_SEPARATOR, module_path);
    found_path = dup_existing_path(full_path);
    if (found_path == NULL) {
#ifndef _WIN32
      snprintf(full_path, sizeof(full_path), "%s%s%s.so",
               paths[i], PATH_SEPARATOR, module_path);
#else
      snprintf(full_path, sizeof(full_path), "%s%s%s.dll",
               paths[i], PATH_SEPARATOR, module_path);
#endif
      found_path = dup_existing_path(full_path);
    }
    if (found_path != NULL) {
      break;
    }
  }

  return found_path;
}

char *find_module_path(const char *module_name) {
  char *found_path = NULL;
  char module_path[256];

  if (is_native_module(module_name)) {
    found_path = strdup("native");
  } else if (has_module_extension(module_name)) {
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
      Environment *module_env = create_environment(env, import_stmt->module_name);
      native_modules[i].init_func(module_env);
      declare_import_items(env, module_env, import_stmt, import_stmt->module_name);
      free_environment(module_env);
      return (RuntimeVal *)MK_NIL();
    }
  }
  error("Native module registry lookup failed.");
  return (RuntimeVal *)MK_NIL();
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

  Environment *module_env = create_environment(env, (char *)so_path);
  init_fn(module_env);

  declare_import_items(env, module_env, import_stmt, so_path);

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
    RuntimeVal *result = eval_import_native(import_stmt, env);
    free_safe(module_path);
    return result;
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

  declare_import_items(env, module_env, import_stmt, import_stmt->module_name);

  free_safe(module_code);
  free_tokens(tokens, token_count);
  free_safe(parser);
  module_env->owned_program = program;
  free_environment(module_env);
  free_safe(module_path);
  return (RuntimeVal *)MK_NIL();
}
