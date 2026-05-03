#include "nm_internal.h"


#define MATH_FUNC_1ARG(name, func)                                             \
  static RuntimeVal *math_##name(Environment *env, RuntimeVal **args,          \
                                 size_t arg_count) {                           \
    if (arg_count != 1 || args[0]->type != NUMBER_T) {                         \
      error(#name "() expects one number argument");                           \
      return (RuntimeVal *)MK_NIL();                                           \
    }                                                                          \
    double value = ((NumberVal *)args[0])->value;                              \
    return (RuntimeVal *)MK_NUMBER(func(value));                               \
  }

#define MATH_FUNC_2ARG(name, func)                                             \
  static RuntimeVal *math_##name(Environment *env, RuntimeVal **args,          \
                                 size_t arg_count) {                           \
    if (arg_count != 2 || args[0]->type != NUMBER_T ||                         \
        args[1]->type != NUMBER_T) {                                           \
      error(#name "() expects two number arguments");                          \
      return (RuntimeVal *)MK_NIL();                                           \
    }                                                                          \
    double value1 = ((NumberVal *)args[0])->value;                             \
    double value2 = ((NumberVal *)args[1])->value;                             \
    return (RuntimeVal *)MK_NUMBER(func(value1, value2));                      \
  }

MATH_FUNC_1ARG(abs, fabs)
MATH_FUNC_1ARG(sqrt, sqrt)
MATH_FUNC_1ARG(sin, sin)
MATH_FUNC_1ARG(cos, cos)
MATH_FUNC_1ARG(tan, tan)
MATH_FUNC_1ARG(log, log)
MATH_FUNC_1ARG(floor, floor)
MATH_FUNC_1ARG(ceil, ceil)
MATH_FUNC_1ARG(round, round)
MATH_FUNC_2ARG(min, fmin)
MATH_FUNC_2ARG(max, fmax)
MATH_FUNC_2ARG(pow, pow)

typedef struct {
  const char *name;
  RuntimeVal *(*func)(Environment *, RuntimeVal **, size_t);
  size_t arg_count;
} MathFunction;

int compare_runtime_vals(const void *a, const void *b) {
  RuntimeVal *val1 = *(RuntimeVal **)a;
  RuntimeVal *val2 = *(RuntimeVal **)b;

  if (val1->type != NUMBER_T || val2->type != NUMBER_T) {
    return 0;
  }

  double num1 = ((NumberVal *)val1)->value;
  double num2 = ((NumberVal *)val2)->value;

  if (num1 < num2)
    return -1;
  if (num1 > num2)
    return 1;
  return 0;
}

static RuntimeVal *math_list_min_max(char *op, Environment *env, RuntimeVal **args,
                                size_t arg_count) {
  if (arg_count != 1 || args[0]->type != LIST_T) {
    char error_message[100];
    snprintf(error_message, sizeof(error_message),
             "%s() expects one list argument.\n", op);
    error(error_message);
  }

  ListVal *list = (ListVal *)args[0];

  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NIL();
  }
  double min_max = ((NumberVal *)list->items[0])->value;
  for (size_t i = 1; i < list->size; i++) {
    if (list->items[i]->type == NUMBER_T) {
      double numVal = ((NumberVal *)list->items[i])->value;
      if (strcmp(op, "min") == 0) {
        if (i == 0 || numVal < min_max) {
          min_max = numVal;
        }
      } else if (strcmp(op, "max") == 0) {
        if (i == 0 || numVal > min_max) {
          min_max = numVal;
        }
      }
    }
  }
  return (RuntimeVal *)MK_NUMBER(min_max);
}

static RuntimeVal *math_list_min(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  return math_list_min_max("min", env, args, arg_count);
}

static RuntimeVal *math_list_max(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  return math_list_min_max("max", env, args, arg_count);
}


static RuntimeVal *math_median(Environment *env, RuntimeVal **args,
                               size_t arg_count) {
  if (arg_count != 1 || args[0]->type != LIST_T) {
    error("median() expects one list argument");
  }

  ListVal *list = (ListVal *)args[0];

  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  RuntimeVal **num_items = (RuntimeVal **)malloc_safe(
      list->size * sizeof(RuntimeVal *), "math_median");
  size_t count = 0;

  for (size_t i = 0; i < list->size; i++) {
    if (list->items[i]->type == NUMBER_T) {
      num_items[count++] = list->items[i];
    }
  }

  if (count == 0) {
    free_safe(num_items);
    return (RuntimeVal *)MK_NUMBER(0);
  }

  qsort(num_items, count, sizeof(RuntimeVal *), compare_runtime_vals);

  double median;
  if (count % 2 == 1) {
    median = ((NumberVal *)num_items[count / 2])->value;
  } else {
    double middle1 = ((NumberVal *)num_items[(count / 2) - 1])->value;
    double middle2 = ((NumberVal *)num_items[count / 2])->value;
    median = (middle1 + middle2) / 2.0;
  }

  free_safe(num_items);

  return (RuntimeVal *)MK_NUMBER(median);
}


static RuntimeVal *math_variance(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  if (arg_count != 1 || args[0]->type != LIST_T) {
    error("variance() expects one list argument");
    return (RuntimeVal *)MK_NIL();
  }

  ListVal *list = (ListVal *)args[0];

  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  double sum = 0.0;
  size_t count = 0;

  for (size_t i = 0; i < list->size; i++) {
    RuntimeVal *item = list->items[i];
    if (item->type == NUMBER_T) {
      sum += ((NumberVal *)item)->value;
      count++;
    }
  }

  if (count == 0) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  double mean = sum / count;
  double squared_diff_sum = 0.0;

  for (size_t i = 0; i < list->size; i++) {
    RuntimeVal *item = list->items[i];
    if (item->type == NUMBER_T) {
      double value = ((NumberVal *)item)->value;
      double diff = value - mean;
      squared_diff_sum += diff * diff;
    }
  }

  return (RuntimeVal *)MK_NUMBER(squared_diff_sum / count);
}
static RuntimeVal *math_average(Environment *env, RuntimeVal **args,
                                size_t arg_count) {
  if (arg_count != 1 || args[0]->type != LIST_T) {
    error("average() expects one list argument");
  }
  ListVal *list = (ListVal *)args[0];

  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  double soma = 0.0;
  size_t count = 0;

  for (size_t i = 0; i < list->size; i++) {
    RuntimeVal *item = list->items[i];
    if (item->type == NUMBER_T) {
      NumberVal *numVal = (NumberVal *)item;
      soma += numVal->value;
      count++;
    }
  }
  return (count > 0) ? (RuntimeVal *)MK_NUMBER(soma / count)
                     : (RuntimeVal *)MK_NUMBER(0);
}

static MathFunction math_functions[] = {
    {"abs", math_abs, 1},     {"sqrt", math_sqrt, 1}, {"sin", math_sin, 1},
    {"cos", math_cos, 1},     {"tan", math_tan, 1},   {"log", math_log, 1},
    {"floor", math_floor, 1}, {"ceil", math_ceil, 1}, {"round", math_round, 1},
    {"min", math_min, 2},     {"max", math_max, 2},   {"pow", math_pow, 2},
    {NULL, NULL, 0}};

void init_math_module(Environment *env) {
  char *single_param[] = {"x"};
  char *double_param[] = {"x", "y"};

  for (MathFunction *func = math_functions; func->name != NULL; func++) {
    char **params = (func->arg_count == 1) ? single_param : double_param;
    declare_owned(
        env, func->name,
        (RuntimeVal *)MK_NATIVE_FN(params, func->arg_count, func->func));
  }
  declare_owned(env, "average",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_average));
  declare_owned(env, "median",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_median));
  declare_owned(env, "variance",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_variance));
  declare_owned(env, "lmin",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_list_min));
  declare_owned(env, "lmax",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_list_max));
}

typedef struct {
  FILE *fp;
  char *mode;
} FileHandle;

