#include "nm_internal.h"

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static RuntimeVal *os_sleep(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1 || args[0]->type != NUMBER_T) error("sleep() expects one number (ms)");
  long ms = (long)((NumberVal *)args[0])->value;
#ifdef _WIN32
  Sleep(ms);
#else
  usleep(ms * 1000);
#endif
  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *os_exit(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1 || args[0]->type != NUMBER_T) error("exit() expects one number");
  exit((int)((NumberVal *)args[0])->value);
  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *os_env(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1 || args[0]->type != STRING_T) error("env() expects one string");
  char *val = getenv(((StringVal *)args[0])->value);
  if (!val) return (RuntimeVal *)MK_NIL();
  return (RuntimeVal *)MK_STRING(val);
}

static RuntimeVal *os_args(Environment *env, RuntimeVal **args, size_t argc) {
  ListVal *list = MK_LIST(zox_argc > 0 ? zox_argc : 1);
  for (int i = 0; i < zox_argc; i++) {
    RuntimeVal *item = (RuntimeVal *)MK_STRING(zox_argv[i]);
    list_append_val(list, item);
    release(item);
  }
  return (RuntimeVal *)list;
}

static RuntimeVal *os_exec(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1 || args[0]->type != STRING_T) error("exec() expects one string");
  char *cmd = ((StringVal *)args[0])->value;
  FILE *fp = popen(cmd, "r");
  if (!fp) return (RuntimeVal *)MK_NIL();

  char *buf = NULL;
  size_t buf_size = 0;
  size_t total = 0;
  char tmp[256];
  while (fgets(tmp, sizeof(tmp), fp)) {
    size_t chunk = strlen(tmp);
    buf = realloc_safe(buf, total + chunk + 1, "os_exec buf");
    memcpy(buf + total, tmp, chunk);
    total += chunk;
    buf_size = total + 1;
  }
  pclose(fp);
  if (!buf) return (RuntimeVal *)MK_STRING("");
  buf[total] = '\0';
  if (total > 0 && buf[total - 1] == '\n') buf[total - 1] = '\0';
  RuntimeVal *r = (RuntimeVal *)MK_STRING(buf);
  free_safe(buf);
  return r;
}

void init_os_module(Environment *env) {
  char *no_params[] = {};
  char *single_param[] = {"a"};

  declare_owned(env, "sleep", (RuntimeVal *)MK_NATIVE_FN(single_param, 1, os_sleep));
  declare_owned(env, "exit", (RuntimeVal *)MK_NATIVE_FN(single_param, 1, os_exit));
  declare_owned(env, "env",  (RuntimeVal *)MK_NATIVE_FN(single_param, 1, os_env));
  declare_owned(env, "args", (RuntimeVal *)MK_NATIVE_FN(no_params,    0, os_args));
  declare_owned(env, "exec", (RuntimeVal *)MK_NATIVE_FN(single_param, 1, os_exec));
}
