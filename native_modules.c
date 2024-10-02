#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "env.h"
#include "global.h"
#include "malloc_safe.h"
#include "values.h"

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
    declare_var(
        env, func->name,
        (RuntimeVal *)MK_NATIVE_FN(params, func->arg_count, func->func));
  }
  declare_var(env, "average",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_average));
  declare_var(env, "median",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_median));
  declare_var(env, "lmin",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_list_min));
  declare_var(env, "lmax",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, math_list_max));
}

typedef struct {
  FILE *fp;
  char *mode;
} FileHandle;

static RuntimeVal *file_open(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  if (arg_count != 2 || args[0]->type != STRING_T ||
      args[1]->type != STRING_T) {
    error("open() expects two string arguments: path and mode");
  }

  char *path = ((StringVal *)args[0])->value;
  char *mode = ((StringVal *)args[1])->value;

  FILE *fp = fopen(path, mode);
  if (!fp) {
    error("Does not possible open the file");
  }

  FileHandle *handle = malloc_safe(sizeof(FileHandle), "FileHandle");
  handle->fp = fp;
  handle->mode = strdup(mode);

  return (RuntimeVal *)handle;
}

static RuntimeVal *file_close(Environment *env, RuntimeVal **args,
                              size_t arg_count) {
  if (arg_count != 1) {
    error("fClose() expect one argument of type file");
  }

  FileHandle *handle = ((FileHandle *)args[0]);

  if (handle->fp == NULL) {
    error("The file is already closed");
  }

  if (strchr(handle->mode, 'w') != NULL || strchr(handle->mode, 'a') != NULL ||
      strchr(handle->mode, '+') != NULL) {
    fflush(handle->fp);
  }
  if (fclose(handle->fp) != 0) {
    error("Error closing the file");
  }
  free_safe(handle->mode);
  free_safe(handle);

  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *file_read(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  if (arg_count != 1) {
    error("fRead() expect one argument of type file");
  }

  FileHandle *handle = (FileHandle *)args[0];
  if (strcmp(handle->mode, "r") != 0 && strcmp(handle->mode, "r+") != 0) {
    error("The file does not open for reading");
  }

  fseek(handle->fp, 0, SEEK_END);
  long fsize = ftell(handle->fp);
  fseek(handle->fp, 0, SEEK_SET);

  char *content = malloc_safe(fsize + 1, "file_read content");
  size_t bytes_read = fread(content, 1, fsize, handle->fp);
  content[bytes_read] = '\0';

  return (RuntimeVal *)MK_STRING(content);
}

static RuntimeVal *file_readline(Environment *env, RuntimeVal **args,
                                 size_t arg_count) {
  if (arg_count != 1) {
    error("fReadLine() expect one argument of type file");
  }

  FileHandle *handle = (FileHandle *)args[0];
  if (strcmp(handle->mode, "r") != 0 && strcmp(handle->mode, "r+") != 0) {
    error("The file does not open for reading");
  }

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  read = getline(&line, &len, handle->fp);
  if (read == -1) {
    free_safe(line);
    return (RuntimeVal *)MK_NUMBER(-1);
  }

  if (read > 0 && line[read - 1] == '\n') {
    line[read - 1] = '\0';
    read--;
  }

  char *utf8_line = malloc_safe(read + 1, "utf8_line");
  memcpy(utf8_line, line, read);
  utf8_line[read] = '\0';

  free_safe(line);

  return (RuntimeVal *)MK_STRING(utf8_line);
}

static RuntimeVal *file_write(Environment *env, RuntimeVal **args,
                              size_t arg_count) {
  if (arg_count != 2) {
    error("fWrite() expect two arguments: file and string");
  }

  FileHandle *handle = (FileHandle *)args[0];
  if (strcmp(handle->mode, "w") != 0 && strcmp(handle->mode, "w+") != 0 &&
      strcmp(handle->mode, "a") != 0 && strcmp(handle->mode, "a+") != 0) {
    error("The file does not open for writing");
  }

  char *content = ((StringVal *)args[1])->value;
  fputs(content, handle->fp);

  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *file_seek(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  if (arg_count != 2 || args[1]->type != NUMBER_T) {
    error("fSeek() expect two arguments: file and number");
  }

  FileHandle *handle = (FileHandle *)args[0];
  long offset = (long)((NumberVal *)args[1])->value;

  fseek(handle->fp, offset, SEEK_SET);

  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *file_exists(Environment *env, RuntimeVal **args,
                               size_t arg_count) {
  if (arg_count != 1 || args[0]->type != STRING_T) {
    error("fExists() expect one arguments: path");
  }
  char *path = ((StringVal *)args[0])->value;
  FILE *file = fopen(path, "r");
  if (file != NULL) {
    fclose(file);
    return (RuntimeVal *)MK_BOOL(1);
  }
  return (RuntimeVal *)MK_BOOL(0);
}

static RuntimeVal *file_delete(Environment *env, RuntimeVal **args,
                               size_t arg_count) {
  if (arg_count != 1 || args[0]->type != STRING_T) {
    error("fDelete() expect one arguments: path");
  }
  char *path = ((StringVal *)args[0])->value;
  if (remove(path) == 0) {
    return (RuntimeVal *)MK_BOOL(1);
  }
  return (RuntimeVal *)MK_BOOL(0);
}

static RuntimeVal *file_move(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  if (arg_count != 2 || args[0]->type != STRING_T ||
      args[1]->type != STRING_T) {
    error("fCopy() expect two path arguments");
  }

  char *path1 = ((StringVal *)args[0])->value;
  char *path2 = ((StringVal *)args[1])->value;

  if (rename(path1, path2) == 0) {
    return (RuntimeVal *)MK_BOOL(1);
  }
  return (RuntimeVal *)MK_BOOL(0);
}

static RuntimeVal *file_copy(Environment *env, RuntimeVal **args,
                             size_t arg_count) {
  if (arg_count != 2 || args[0]->type != STRING_T ||
      args[1]->type != STRING_T) {
    error("fCopy() expect two path arguments");
  }

  char *path1 = ((StringVal *)args[0])->value;
  char *path2 = ((StringVal *)args[1])->value;

  FILE *source, *destination;
  char *buffer;
  size_t bufferSize = 32 * 1024;
  size_t bytesRead;

  buffer = (char *)malloc_safe(bufferSize, "file_copy");

  source = fopen(path1, "rb");
  if (source == NULL) {
    error("Does not possible open source the file");
    free_safe(buffer);
    return (RuntimeVal *)MK_BOOL(0);
  }

  destination = fopen(path2, "wb");
  if (destination == NULL) {
    error("Does not possible open destination the file");
    fclose(source);
    free_safe(buffer);
    return (RuntimeVal *)MK_BOOL(0);
  }

  while ((bytesRead = fread(buffer, 1, bufferSize, source)) > 0) {
    fwrite(buffer, 1, bytesRead, destination);
  }

  fclose(source);
  fclose(destination);

  free_safe(buffer);

  return (RuntimeVal *)MK_BOOL(1);
}

void init_file_module(Environment *env) {
  char *double_param[] = {"f", "n"};
  char *single_param[] = {"f"};

  declare_var(env, "open",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_open));
  declare_var(env, "fRead",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_read));
  declare_var(env, "fExists",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_delete));
  declare_var(env, "fDelete",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_exists));
  declare_var(env, "fReadLine",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_readline));
  declare_var(env, "fWrite",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_write));
  declare_var(env, "fSeek",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_seek));
  declare_var(env, "fCopy",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_copy));
  declare_var(env, "fMove",
              (RuntimeVal *)MK_NATIVE_FN(double_param, 2, file_move));
  declare_var(env, "fClose",
              (RuntimeVal *)MK_NATIVE_FN(single_param, 1, file_close));
}