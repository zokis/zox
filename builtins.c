/* Builtin functions registration. */
#include "builtins.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "eval.h"
#include "global.h"
#include "values.h"
#include "zox_alloc.h"

extern char *runtime_value_to_string(RuntimeVal *val);

static char *builtin_key_arg(RuntimeVal *arg, int *should_free) {
  if (arg->type == STRING_T) {
    *should_free = 0;
    return ((StringVal *)arg)->value;
  }
  *should_free = 1;
  char *key = dict_key_to_string(arg);
  if (!key) error("Dict key must be convertible to string.");
  return key;
}

static RuntimeVal *result_dict_with_key(const char *key, RuntimeVal *value) {
  DictVal *res = MK_DICT(1);
  dict_set_val(res, key, value);
  return (RuntimeVal *)res;
}

static RuntimeVal *result_lookup_or_default(RuntimeVal *arg, const char *key,
                                            RuntimeVal *default_val) {
  if (arg->type != DICT_T) return default_val;
  RuntimeVal *val = dict_get_val((DictVal *)arg, key);
  return val ? val : default_val;
}

RuntimeVal *builtin_sum(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != LIST_T)
    error("The 'sum' function expects exactly one list argument.");

  ListVal *list = (ListVal *)args[0];
  double   total = 0;
  for (size_t i = 0; i < list->size; i++) {
    if (list->items[i]->type != NUMBER_T)
      error("The 'sum' function only supports lists of numbers.");
    total += ((NumberVal *)list->items[i])->value;
  }
  return (RuntimeVal *)MK_NUMBER(total);
}

static RuntimeVal *find_in_list(ListVal *list, RuntimeVal *target) {
  for (size_t i = 0; i < list->size; i++) {
    if (compare_runtimeval(list->items[i], target))
      return (RuntimeVal *)MK_NUMBER((double)i);
  }
  return (RuntimeVal *)MK_NUMBER(-1.0);
}

static RuntimeVal *find_in_string(StringVal *str, RuntimeVal *target) {
  if (target->type != STRING_T)
    error("The 'find' function on a string expects a string search value.");
  const char *haystack = str->value;
  const char *needle   = ((StringVal *)target)->value;
  char *pos = strstr(haystack, needle);
  if (pos) return (RuntimeVal *)MK_NUMBER((double)(pos - haystack));
  return (RuntimeVal *)MK_NUMBER(-1.0);
}

RuntimeVal *builtin_find(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type == LIST_T) {
    return find_in_list((ListVal *)args[0], args[1]);
  } else if (args[0]->type == STRING_T) {
    return find_in_string((StringVal *)args[0], args[1]);
  } else {
    error("The 'find' function is only supported for lists and strings.");
  }
  return (RuntimeVal *)MK_NUMBER(-1.0);
}

RuntimeVal *builtin_keys(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != DICT_T)
    error("The 'keys' function expects exactly one dictionary argument.");

  return (RuntimeVal *)dict_to_keys((DictVal *)args[0]);
}

RuntimeVal *builtin_values(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != DICT_T)
    error("The 'values' function expects exactly one dictionary argument.");

  DictVal *dict = (DictVal *)args[0];
  ListVal *vals = MK_LIST(dict->size);
  for (size_t i = 0; i < dict->capacity; i++) {
    if (dict->entries[i].key != NULL) {
      vals->items[vals->size++] = dict->entries[i].value;
      retain(dict->entries[i].value);
    }
  }
  return (RuntimeVal *)vals;
}

RuntimeVal *builtin_has_key(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != DICT_T)
    error("The 'has_key' function expects a dictionary and a string key.");

  int should_free;
  char *key = builtin_key_arg(args[1], &should_free);
  RuntimeVal *result = (RuntimeVal *)MK_BOOL(dict_find_entry((DictVal *)args[0], key) != NULL);
  if (should_free) zox_free_buf(ZOX_BUF_DICT_KEY, key);
  return result;
}

RuntimeVal *builtin_get(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != DICT_T)
    error("The 'get' function expects a dictionary and a string key.");

  int should_free;
  char *key = builtin_key_arg(args[1], &should_free);
  RuntimeVal *val = dict_get_val((DictVal *)args[0], key);
  if (should_free) zox_free_buf(ZOX_BUF_DICT_KEY, key);
  return val ? val : (RuntimeVal *)MK_NIL();
}

RuntimeVal *builtin_setdefault(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != DICT_T)
    error("The 'setdefault' function expects a dictionary, a key, and a default value.");

  int should_free;
  char *key = builtin_key_arg(args[1], &should_free);
  DictVal *dict = (DictVal *)args[0];
  RuntimeVal *val = dict_get_val(dict, key);
  if (!val) {
    dict_set_val(dict, key, args[2]);
    val = args[2];
    retain(val);
  }
  if (should_free) zox_free_buf(ZOX_BUF_DICT_KEY, key);
  return val;
}

RuntimeVal *builtin_len(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  switch (args[0]->type) {
  case LIST_T:   return (RuntimeVal *)MK_NUMBER((double)((ListVal *)args[0])->size);
  case DICT_T:   return (RuntimeVal *)MK_NUMBER((double)((DictVal *)args[0])->size);
  case STRUCT_T: return (RuntimeVal *)MK_NUMBER((double)((StructVal *)args[0])->type_def->field_count);
  case STRING_T: return (RuntimeVal *)MK_NUMBER((double)strlen(((StringVal *)args[0])->value));
  default:       error("len() is only supported for lists, dictionaries, and strings.");
  }
  return (RuntimeVal *)MK_NIL();
}

void _builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count, int newline) {
  (void)env;
  for (size_t i = 0; i < arg_count; i++) {
    char *str = runtime_value_to_string(args[i]);
    if (str) {
      printf("%s", str);
      zox_free_buf(ZOX_BUF_TEMP, str);
    } else {
      printf("nil");
    }
    if (i < arg_count - 1) printf(" ");
  }
  if (newline) printf("\n");
}

RuntimeVal *builtin_println_value(Environment *env, RuntimeVal **args, size_t arg_count) {
  _builtin_print_value(env, args, arg_count, 1);
  return (RuntimeVal *)MK_NIL();
}

RuntimeVal *builtin_typeof(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type == STRUCT_T) {
    StructVal *sv = (StructVal *)args[0];
    char buf[256];
    snprintf(buf, sizeof(buf), "type<%s>", sv->type_def->name);
    return (RuntimeVal *)MK_STRING(buf);
  }
  return (RuntimeVal *)MK_STRING(type_to_string(args[0]->type));
}

static RuntimeVal *copy_list(ListVal *old) {
  ListVal *new = MK_LIST(old->size);
  for (size_t i = 0; i < old->size; i++) {
    new->items[i] = old->items[i];
    retain(new->items[i]);
  }
  new->size = old->size;
  return (RuntimeVal *)new;
}

static RuntimeVal *copy_dict(DictVal *old) {
  DictVal *new = MK_DICT(old->capacity);
  for (size_t i = 0; i < old->capacity; i++) {
    if (old->entries[i].key) {
      dict_set_val(new, old->entries[i].key, old->entries[i].value);
    }
  }
  return (RuntimeVal *)new;
}

static RuntimeVal *copy_struct(StructVal *old) {
  size_t count = old->type_def->field_count;
  RuntimeVal *inline_values[4];
  RuntimeVal **values = count <= 4
      ? inline_values
      : zox_alloc_buf(
          ZOX_BUF_STRUCT_VALUES, sizeof(RuntimeVal *) * count,
          "struct copy values");
  for (size_t i = 0; i < old->type_def->field_count; i++) {
    values[i] = old->values[i];
    retain(values[i]);
  }
  RuntimeVal *copy = (RuntimeVal *)MK_STRUCT_COPY_VALUES(old->type_def, values);
  if (count > 4) {
    zox_free_buf(ZOX_BUF_STRUCT_VALUES, values);
  }
  return copy;
}

RuntimeVal *builtin_copy(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  RuntimeVal *val = args[0];
  switch (val->type) {
    case LIST_T:   return copy_list((ListVal *)val);
    case DICT_T:   return copy_dict((DictVal *)val);
    case STRUCT_T: return copy_struct((StructVal *)val);
    default:       retain(val); return val;
  }
}

RuntimeVal *builtin_random(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)args; (void)arg_count;
  return (RuntimeVal *)MK_NUMBER((double)rand() / (double)RAND_MAX);
}

RuntimeVal *builtin_random_int(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != NUMBER_T || args[1]->type != NUMBER_T)
    error("random_int(min, max) expects two numbers");
  int min = (int)((NumberVal *)args[0])->value;
  int max = (int)((NumberVal *)args[1])->value;
  if (min > max) error("min cannot be greater than max in random_int");
  return (RuntimeVal *)MK_NUMBER((double)(min + rand() % (max - min + 1)));
}

RuntimeVal *builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)arg_count;
  _builtin_print_value(env, args, 1, 0);
  return (RuntimeVal *)MK_NIL();
}

RuntimeVal *builtin_ok(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  return result_dict_with_key("ok", args[0]);
}

RuntimeVal *builtin_err(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  return result_dict_with_key("err", args[0]);
}

RuntimeVal *builtin_is_ok(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != DICT_T) return (RuntimeVal *)MK_BOOL(0);
  return (RuntimeVal *)MK_BOOL(dict_find_entry((DictVal *)args[0], "ok") != NULL);
}

RuntimeVal *builtin_is_err(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  if (args[0]->type != DICT_T) return (RuntimeVal *)MK_BOOL(0);
  return (RuntimeVal *)MK_BOOL(dict_find_entry((DictVal *)args[0], "err") != NULL);
}

RuntimeVal *builtin_get_ok(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  return result_lookup_or_default(args[0], "ok", (RuntimeVal *)MK_NIL());
}

RuntimeVal *builtin_get_err(Environment *env, RuntimeVal **args, size_t arg_count) {
  (void)env; (void)arg_count;
  return result_lookup_or_default(args[0], "err", (RuntimeVal *)MK_NIL());
}

static char *no_params[]     = {NULL};
static char *single_param[]  = {"value"};
static char *double_param[]  = {"param1", "param2"};
static char *triple_param[]  = {"param1", "param2", "param3"};

static void register_collection_builtins(Environment *env) {
  declare_owned(env, "keys", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_keys));
  declare_owned(env, "values", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_values));
  declare_owned(env, "has_key", (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_has_key));
  declare_owned(env, "get", (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_get));
  declare_owned(env, "setdefault", (RuntimeVal *)MK_FUNCTION(triple_param, 3, NULL, 0, NULL, builtin_setdefault));
  declare_owned(env, "len", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_len));
  declare_owned(env, "sum", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_sum));
  declare_owned(env, "find", (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_find));
}

static void register_io_builtins(Environment *env) {
  declare_owned(env, "print", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_print_value));
  declare_owned(env, "println", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_println_value));
}

static void register_utility_builtins(Environment *env) {
  declare_owned(env, "random_int", (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_random_int));
  declare_owned(env, "random", (RuntimeVal *)MK_FUNCTION(no_params, 0, NULL, 0, NULL, builtin_random));
  declare_owned(env, "typeof", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_typeof));
  declare_owned(env, "copy", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_copy));
}

static void register_result_builtins(Environment *env) {
  declare_owned(env, "ok", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_ok));
  declare_owned(env, "err", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_err));
  declare_owned(env, "is_ok", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_is_ok));
  declare_owned(env, "is_err", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_is_err));
  declare_owned(env, "get_ok", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_get_ok));
  declare_owned(env, "get_err", (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_get_err));
}

void register_builtins(Environment *env) {
  register_collection_builtins(env);
  register_io_builtins(env);
  register_utility_builtins(env);
  register_result_builtins(env);
}
