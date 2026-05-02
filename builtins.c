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
#include "malloc_safe.h"
#include "values.h"

extern char *runtime_value_to_string(RuntimeVal *val);

RuntimeVal *builtin_sum(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1 || args[0]->type != LIST_T)
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

RuntimeVal *builtin_find(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 2)
    error("The 'find' function expects two arguments (collection and value).");

  if (args[0]->type == LIST_T) {
    ListVal *list = (ListVal *)args[0];
    for (size_t i = 0; i < list->size; i++) {
      if (compare_runtimeval(list->items[i], args[1]))
        return (RuntimeVal *)MK_NUMBER((double)i);
    }
  } else if (args[0]->type == STRING_T) {
    if (args[1]->type != STRING_T)
      error("The 'find' function on a string expects a string search value.");
    const char *haystack = ((StringVal *)args[0])->value;
    const char *needle   = ((StringVal *)args[1])->value;
    char *pos = strstr(haystack, needle);
    if (pos) return (RuntimeVal *)MK_NUMBER((double)(pos - haystack));
  } else {
    error("The 'find' function is only supported for lists and strings.");
  }
  return (RuntimeVal *)MK_NUMBER(-1.0);
}

RuntimeVal *builtin_keys(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1 || args[0]->type != DICT_T)
    error("The 'keys' function expects exactly one dictionary argument.");

  return (RuntimeVal *)dict_to_keys((DictVal *)args[0]);
}

RuntimeVal *builtin_values(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1 || args[0]->type != DICT_T)
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
  if (arg_count != 2 || args[0]->type != DICT_T)
    error("The 'has_key' function expects a dictionary and a string key.");

  char *key = runtime_value_to_string(args[1]);
  if (!key) error("Dict key must be convertible to string.");
  RuntimeVal *result = (RuntimeVal *)MK_BOOL(dict_find_entry((DictVal *)args[0], key) != NULL);
  free_safe(key);
  return result;
}

RuntimeVal *builtin_get(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 2 || args[0]->type != DICT_T)
    error("The 'get' function expects a dictionary and a string key.");

  char *key = runtime_value_to_string(args[1]);
  if (!key) error("Dict key must be convertible to string.");
  RuntimeVal *val = dict_get_val((DictVal *)args[0], key);
  free_safe(key);
  return val ? val : (RuntimeVal *)MK_NIL();
}

RuntimeVal *builtin_setdefault(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 3 || args[0]->type != DICT_T)
    error("The 'setdefault' function expects a dictionary, a key, and a default value.");

  char *key = runtime_value_to_string(args[1]);
  if (!key) error("Dict key must be convertible to string.");
  DictVal *dict = (DictVal *)args[0];
  RuntimeVal *val = dict_get_val(dict, key);
  if (!val) {
    dict_set_val(dict, key, args[2]);
    val = args[2];
    retain(val);
  }
  free_safe(key);
  return val;
}

RuntimeVal *builtin_len(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("len() function expects exactly one argument.");

  switch (args[0]->type) {
  case LIST_T:   return (RuntimeVal *)MK_NUMBER((double)((ListVal *)args[0])->size);
  case DICT_T:   return (RuntimeVal *)MK_NUMBER((double)((DictVal *)args[0])->size);
  case STRING_T: return (RuntimeVal *)MK_NUMBER((double)strlen(((StringVal *)args[0])->value));
  default:       error("len() is only supported for lists, dictionaries, and strings.");
  }
  return (RuntimeVal *)MK_NIL();
}

void _builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count, int newline) {
  for (size_t i = 0; i < arg_count; i++) {
    char *str = runtime_value_to_string(args[i]);
    if (str) {
      printf("%s", str);
      free_safe(str);
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
  if (arg_count != 1) error("typeof() expects one argument");
  return (RuntimeVal *)MK_STRING(type_to_string(args[0]->type));
}

RuntimeVal *builtin_copy(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("copy() expects one argument");
  RuntimeVal *val = args[0];
  if (val->type == LIST_T) {
    ListVal *old = (ListVal *)val;
    ListVal *new = MK_LIST(old->size);
    for (size_t i = 0; i < old->size; i++) {
      new->items[i] = old->items[i];
      retain(new->items[i]);
    }
    new->size = old->size;
    return (RuntimeVal *)new;
  }
  if (val->type == DICT_T) {
    DictVal *old = (DictVal *)val;
    DictVal *new = MK_DICT(old->capacity);
    for (size_t i = 0; i < old->capacity; i++) {
      if (old->entries[i].key) {
        dict_set_val(new, old->entries[i].key, old->entries[i].value);
      }
    }
    return (RuntimeVal *)new;
  }
  retain(val);
  return val;
}

RuntimeVal *builtin_random(Environment *env, RuntimeVal **args, size_t arg_count) {
  return (RuntimeVal *)MK_NUMBER((double)rand() / (double)RAND_MAX);
}

RuntimeVal *builtin_random_int(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 2 || args[0]->type != NUMBER_T || args[1]->type != NUMBER_T)
    error("random_int(min, max) expects two numbers");
  int min = (int)((NumberVal *)args[0])->value;
  int max = (int)((NumberVal *)args[1])->value;
  if (min > max) error("min cannot be greater than max in random_int");
  return (RuntimeVal *)MK_NUMBER((double)(min + rand() % (max - min + 1)));
}

RuntimeVal *builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count) {
  _builtin_print_value(env, args, 1, 0);
  return (RuntimeVal *)MK_NIL();
}

RuntimeVal *builtin_ok(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("ok() expects exactly one argument.");
  DictVal *res = MK_DICT(1);
  dict_set_val(res, "ok", args[0]);
  return (RuntimeVal *)res;
}

RuntimeVal *builtin_err(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("err() expects exactly one argument.");
  DictVal *res = MK_DICT(1);
  dict_set_val(res, "err", args[0]);
  return (RuntimeVal *)res;
}

void register_builtins(Environment *env) {
  static char *no_params[]     = {NULL};
  static char *single_param[]  = {"value"};
  static char *double_param[]  = {"param1", "param2"};
  static char *triple_param[]  = {"param1", "param2", "param3"};

  declare_owned(env, "keys",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_keys));
  declare_owned(env, "has_key",
    (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_has_key));
  declare_owned(env, "get",
    (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_get));
  declare_owned(env, "setdefault",
    (RuntimeVal *)MK_FUNCTION(triple_param, 3, NULL, 0, NULL, builtin_setdefault));
  declare_owned(env, "len",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_len));
  declare_owned(env, "print",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_print_value));
  declare_owned(env, "println",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_println_value));
  declare_owned(env, "values",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_values));
  declare_owned(env, "sum",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_sum));
  declare_owned(env, "random_int",
    (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_random_int));
  declare_owned(env, "find",
    (RuntimeVal *)MK_FUNCTION(double_param, 2, NULL, 0, NULL, builtin_find));
  declare_owned(env, "random",
    (RuntimeVal *)MK_FUNCTION(no_params, 0, NULL, 0, NULL, builtin_random));
  declare_owned(env, "typeof",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_typeof));
  declare_owned(env, "copy",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_copy));
  declare_owned(env, "ok",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_ok));
  declare_owned(env, "err",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_err));
}
