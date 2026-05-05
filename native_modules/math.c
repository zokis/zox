#include "nm_internal.h"


#define MATH_FUNC_1ARG(name, func)                                             \
  static RuntimeVal *math_##name(Environment *env, RuntimeVal **args,          \
                                 size_t arg_count) {                           \
    (void)env;                                                                 \
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
    (void)env;                                                                 \
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

  double diff = ((NumberVal *)val1)->value - ((NumberVal *)val2)->value;
  return (diff > 0) - (diff < 0);
}

static RuntimeVal *math_list_min_max(int is_min, const char *op, Environment *env,
                                     RuntimeVal **args, size_t arg_count) {
  (void)env;
  if (arg_count != 1 || args[0]->type != LIST_T) {
    char error_message[100];
    snprintf(error_message, sizeof(error_message),
             "%s() expects one list argument.\n", op);
    error(error_message);
    return (RuntimeVal *)MK_NIL();
  }

  ListVal *list = (ListVal *)args[0];
  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NIL();
  }

  size_t list_size = list->size;
  RuntimeVal **items = list->items;
  double extreme = 0.0;
  int found = 0;

  for (size_t i = 0; i < list_size; i++) {
    RuntimeVal *item = items[i];
    if (item->type != NUMBER_T) continue;

    double value = ((NumberVal *)item)->value;
    if (!found || (is_min ? value < extreme : value > extreme)) {
      extreme = value;
      found = 1;
    }
  }

  return found ? (RuntimeVal *)MK_NUMBER(extreme) : (RuntimeVal *)MK_NIL();
}

static RuntimeVal *math_list_min(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  return math_list_min_max(1, "min", env, args, arg_count);
}

static RuntimeVal *math_list_max(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  return math_list_min_max(0, "max", env, args, arg_count);
}


static RuntimeVal *math_median(Environment *env, RuntimeVal **args,
                               size_t arg_count) {
  (void)env;
  if (arg_count != 1 || args[0]->type != LIST_T) {
    error("median() expects one list argument");
  }

  ListVal *list = (ListVal *)args[0];
  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  size_t list_size = list->size;
  RuntimeVal **num_items = (RuntimeVal **)zox_alloc_buf(
      ZOX_BUF_TEMP, list_size * sizeof(RuntimeVal *), "math_median");
  size_t count = 0;

  for (size_t i = 0; i < list_size; i++) {
    if (list->items[i]->type == NUMBER_T) {
      num_items[count++] = list->items[i];
    }
  }

  if (count == 0) {
    zox_free_buf(ZOX_BUF_TEMP, num_items);
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

  zox_free_buf(ZOX_BUF_TEMP, num_items);

  return (RuntimeVal *)MK_NUMBER(median);
}


static RuntimeVal *math_variance(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  (void)env;
  if (arg_count != 1 || args[0]->type != LIST_T) {
    error("variance() expects one list argument");
  }

  ListVal *list = (ListVal *)args[0];
  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  size_t list_size = list->size;
  RuntimeVal **items = list->items;
  double mean = 0.0;
  double squared_diff_sum = 0.0;
  size_t count = 0;

  for (size_t i = 0; i < list_size; i++) {
    RuntimeVal *item = items[i];
    if (item->type == NUMBER_T) {
      double value = ((NumberVal *)item)->value;
      count++;
      double delta = value - mean;
      mean += delta / count;
      squared_diff_sum += delta * (value - mean);
    }
  }

  if (count == 0) return (RuntimeVal *)MK_NUMBER(0);

  return (RuntimeVal *)MK_NUMBER(squared_diff_sum / count);
}

static RuntimeVal *math_standard_deviation(Environment *env, RuntimeVal **args,
                                          size_t arg_count) {
  RuntimeVal *variance_result = math_variance(env, args, arg_count);
  if (variance_result->type != NUMBER_T) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  double variance = ((NumberVal *)variance_result)->value;
  return (RuntimeVal *)MK_NUMBER(sqrt(variance));
}

static RuntimeVal *math_correlation(Environment *env, RuntimeVal **args,
                                    size_t arg_count) {
  (void)env;
  if (arg_count != 2 || args[0]->type != LIST_T || args[1]->type != LIST_T) {
    error("correlation() expects two list arguments");
  }

  ListVal *x = (ListVal *)args[0];
  ListVal *y = (ListVal *)args[1];
  size_t n = (x->size < y->size) ? x->size : y->size;

  if (n == 0) return (RuntimeVal *)MK_NUMBER(0);

  double mean_x = 0.0, mean_y = 0.0;
  double sum_sq_x = 0.0, sum_sq_y = 0.0, sum_xy = 0.0;
  size_t count = 0;
  RuntimeVal **x_items = x->items;
  RuntimeVal **y_items = y->items;

  for (size_t i = 0; i < n; i++) {
    if (x_items[i]->type == NUMBER_T && y_items[i]->type == NUMBER_T) {
      double value_x = ((NumberVal *)x_items[i])->value;
      double value_y = ((NumberVal *)y_items[i])->value;

      count++;
      double delta_x = value_x - mean_x;
      double delta_y = value_y - mean_y;
      mean_x += delta_x / count;
      mean_y += delta_y / count;
      sum_sq_x += delta_x * (value_x - mean_x);
      sum_sq_y += delta_y * (value_y - mean_y);
      sum_xy += delta_x * (value_y - mean_y);
    }
  }

  if (count == 0 || sum_sq_x == 0.0 || sum_sq_y == 0.0) return (RuntimeVal *)MK_NUMBER(0);

  return (RuntimeVal *)MK_NUMBER(sum_xy / sqrt(sum_sq_x * sum_sq_y));
}

static RuntimeVal *math_gcd(Environment *env, RuntimeVal **args,
                            size_t arg_count) {
  (void)env;
  if (arg_count != 2 || args[0]->type != NUMBER_T || args[1]->type != NUMBER_T) {
    error("gcd() expects two number arguments");
    return (RuntimeVal *)MK_NIL();
  }

  long long a = (long long)llabs((long long)((NumberVal *)args[0])->value);
  long long b = (long long)llabs((long long)((NumberVal *)args[1])->value);

  while (b != 0) {
    long long t = b;
    b = a % b;
    a = t;
  }

  return (RuntimeVal *)MK_NUMBER((double)a);
}

static RuntimeVal *math_average(Environment *env, RuntimeVal **args,
                                size_t arg_count) {
  (void)env;
  if (arg_count != 1 || args[0]->type != LIST_T) {
    error("average() expects one list argument");
  }
  ListVal *list = (ListVal *)args[0];
  if (list == NULL || list->size == 0) {
    return (RuntimeVal *)MK_NUMBER(0);
  }

  size_t list_size = list->size;
  RuntimeVal **items = list->items;
  double soma = 0.0;
  size_t count = 0;

  for (size_t i = 0; i < list_size; i++) {
    RuntimeVal *item = items[i];
    if (item->type == NUMBER_T) {
      soma += ((NumberVal *)item)->value;
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


  
  declare_owned(env, "standardDeviation",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_standard_deviation));
  declare_owned(env, "correlation",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, math_correlation));
  declare_owned(env, "gcd",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, math_gcd));
  declare_owned(env, "lmin",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_list_min));
  declare_owned(env, "lmax",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_list_max));
}

typedef struct {
  FILE *fp;
  char *mode;
} FileHandle;
