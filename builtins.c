#include "builtins.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "eval.h"
#include "global.h"
#include "hash.h"
#include "malloc_safe.h"
#include "values.h"

RuntimeVal *builtin_sum(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1 || args[0]->type != LIST_T)
    error("The 'sum' function expects exactly one list argument.");
  ListVal *list = (ListVal *)args[0];
  double total = 0.0;
  for (size_t i = 0; i < list->size; i++) {
    if (list->items[i]->type != NUMBER_T) error("All elements of the list must be numbers.");
    total += ((NumberVal *)list->items[i])->value;
  }
  return (RuntimeVal *)MK_NUMBER(total);
}

RuntimeVal *builtin_find(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 2) error("Function 'find' expects exactly two arguments.");
  if (args[0]->type != STRING_T && args[0]->type != LIST_T && args[0]->type != DICT_T)
    error("The first argument for 'find' must be a string, list or dictionary.");
  if (args[1]->type != STRING_T && args[1]->type != NUMBER_T && args[1]->type != BOOLEAN_T)
    error("The second argument for 'find' must be a string, number or boolean.");
  if (args[0]->type == STRING_T) {
    char *pos = strstr(((StringVal *)args[0])->value, ((StringVal *)args[1])->value);
    if (pos != NULL)
      return (RuntimeVal *)MK_NUMBER((double)(pos - ((StringVal *)args[0])->value));
  }
  if (args[0]->type == LIST_T || args[0]->type == DICT_T) {
    ListVal *obj = (args[0]->type == LIST_T) ? (ListVal *)args[0]
                                              : (ListVal *)dict_to_keys((DictVal *)args[0]);
    for (size_t i = 0; i < obj->size; i++) {
      if (compare_runtimeval(obj->items[i], args[1]))
        return (RuntimeVal *)MK_NUMBER((double)i);
    }
  }
  return (RuntimeVal *)MK_NUMBER((double)-1);
}

RuntimeVal *builtin_keys(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("Function 'keys' expects exactly one argument.");
  if (args[0]->type != DICT_T) error("Argument to 'keys' must be a dictionary.");
  return (RuntimeVal *)dict_to_keys((DictVal *)args[0]);
}

RuntimeVal *builtin_values(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("The 'values' function expects exactly one argument.");
  if (args[0]->type != DICT_T) error("The argument for 'values' must be a dictionary.");
  DictVal *dict = (DictVal *)args[0];
  ListVal *values_list = MK_LIST(dict->size);
  for (size_t i = 0; i < dict->capacity; i++) {
    for (Entry *e = dict->entries[i]; e != NULL; e = e->next)
      values_list->items[values_list->size++] = e->value;
  }
  return (RuntimeVal *)values_list;
}

RuntimeVal *builtin_len(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("Function 'len' expects exactly one argument.");
  if (args[0]->type == LIST_T)
    return (RuntimeVal *)MK_NUMBER((double)((ListVal *)args[0])->size);
  if (args[0]->type == STRING_T)
    return (RuntimeVal *)MK_NUMBER((double)strlen(((StringVal *)args[0])->value));
  if (args[0]->type == DICT_T)
    return (RuntimeVal *)MK_NUMBER((double)((DictVal *)args[0])->size);
  error("Argument to 'len' must be a list, string or dictionary.");
  return NULL;
}

RuntimeVal *builtin_typeof(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("Function 'typeof' expects exactly one argument.");
  return (RuntimeVal *)MK_STRING(type_to_string(args[0]->type));
}

/* Mutable values deep-copy; immutable values/functions share with retain. */
static RuntimeVal *deep_copy(RuntimeVal *val) {
  switch (val->type) {
  case NUMBER_T:
  case STRING_T:
  case BOOLEAN_T:
  case NIL_T:
  case FUNCTION_T:
    retain(val);
    return val;
  case LIST_T: {
    ListVal *src  = (ListVal *)val;
    ListVal *dst  = MK_LIST(src->capacity > 0 ? src->capacity : 1);
    for (size_t i = 0; i < src->size; i++) {
      RuntimeVal *item = deep_copy(src->items[i]);
      list_append_val(dst, item);
      release(item);
    }
    return (RuntimeVal *)dst;
  }
  case DICT_T: {
    DictVal *src = (DictVal *)val;
    DictVal *dst = MK_DICT(src->capacity > 0 ? src->capacity : 1);
    for (size_t i = 0; i < src->capacity; i++) {
      for (Entry *e = src->entries[i]; e != NULL; e = e->next) {
        RuntimeVal *v = deep_copy(e->value);
        dict_set_val(dst, e->key, v);
        release(v);
      }
    }
    return (RuntimeVal *)dst;
  }
  default:
    retain(val);
    return val;
  }
}

RuntimeVal *builtin_copy(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 1) error("Function 'copy' expects exactly one argument.");
  return deep_copy(args[0]);
}

void _builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count, bool as_string) {
  if (arg_count != 1) error("Function 'print' expects exactly one argument.");
  RuntimeVal *val = args[0];
  switch (val->type) {
  case NIL_T:
    printf("nil");
    break;
  case BOOLEAN_T:
    printf("%s", ((BooleanVal *)val)->value ? "true" : "false");
    break;
  case NUMBER_T:
    printf("%g", ((NumberVal *)val)->value);
    break;
  case STRING_T:
    if (as_string) printf("\"%s\"", ((StringVal *)val)->value);
    else           printf("%s",    ((StringVal *)val)->value);
    break;
  case LIST_T: {
    ListVal *list_val = (ListVal *)val;
    printf("{");
    for (size_t i = 0; i < list_val->size; i++) {
      if (i > 0) printf(", ");
      RuntimeVal *item_args[] = {list_val->items[i]};
      _builtin_print_value(env, item_args, 1, 1);
    }
    printf("}");
    break;
  }
  case DICT_T: {
    DictVal *dict_val = (DictVal *)val;
    printf("[");
    short int first = 1;
    for (size_t i = 0; i < dict_val->capacity; i++) {
      for (Entry *e = dict_val->entries[i]; e != NULL; e = e->next) {
        if (!first) printf("; ");
        printf("\"%s\" -> ", e->key);
        RuntimeVal *entry_args[] = {e->value};
        _builtin_print_value(env, entry_args, 1, 1);
        first = 0;
      }
    }
    printf("]");
    break;
  }
  case FUNCTION_T:
    printf("<function>");
    break;
  default:
    printf("Unknown value type\n");
  }
}

RuntimeVal *builtin_println_value(Environment *env, RuntimeVal **args, size_t arg_count) {
  _builtin_print_value(env, args, 1, 0);
  printf("\n");
  return (RuntimeVal *)MK_NIL();
}

RuntimeVal *builtin_random(Environment *env, RuntimeVal **args, size_t arg_count) {
  static int initialized = 0;
  if (!initialized) { srand(time(NULL)); initialized = 1; }
  return (RuntimeVal *)MK_NUMBER((double)rand() / RAND_MAX);
}

RuntimeVal *builtin_random_int(Environment *env, RuntimeVal **args, size_t arg_count) {
  if (arg_count != 2) {
    fprintf(stderr, "Error: random_int expects 2 arguments, but received %zu\n", arg_count);
    return (RuntimeVal *)MK_NIL();
  }
  if (args[0]->type != NUMBER_T || args[1]->type != NUMBER_T) {
    fprintf(stderr, "Error: random_int expects two numbers as arguments\n");
    return (RuntimeVal *)MK_NIL();
  }
  int min = (int)((NumberVal *)args[0])->value;
  int max = (int)((NumberVal *)args[1])->value;
  if (min > max) {
    fprintf(stderr, "Error: the first argument must be less than or equal to the second\n");
    return (RuntimeVal *)MK_NIL();
  }
  static int initialized = 0;
  if (!initialized) { srand(time(NULL)); initialized = 1; }
  return (RuntimeVal *)MK_NUMBER((double)(min + rand() % (max - min + 1)));
}

RuntimeVal *builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count) {
  _builtin_print_value(env, args, 1, 0);
  return (RuntimeVal *)MK_NIL();
}

void register_builtins(Environment *env) {
  char *no_params[]    = {};
  char *single_param[] = {"value"};
  char *double_param[] = {"param1", "param2"};

  declare_owned(env, "keys",
    (RuntimeVal *)MK_FUNCTION(single_param, 1, NULL, 0, NULL, builtin_keys));
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
}
