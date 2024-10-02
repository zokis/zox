#include "ast.h"
#include "env.h"
#include <stddef.h>

#ifndef VALUE_H
#define VALUE_H

typedef struct Environment Environment;  // Forward declaration

typedef enum {
  NIL_T,
  NUMBER_T,
  BOOLEAN_T,
  STRING_T,
  LIST_T,
  DICT_T,
  TABLE_T,
  FUNCTION_T
} ValueType;

typedef struct {
  ValueType type;
} RuntimeVal;

typedef struct {
  RuntimeVal base;
} NilVal;

typedef struct {
  RuntimeVal base;
  short int value;
} BooleanVal;

typedef struct {
  RuntimeVal base;
  double value;
} NumberVal;

typedef struct {
  RuntimeVal base;
  char *value;
} StringVal;

typedef struct {
  RuntimeVal base;
  char **params;
  size_t param_count;
  Stmt **body;
  size_t body_count;
  Environment *env;
  RuntimeVal *(*builtin_func)(Environment *env, RuntimeVal **args,
                              size_t arg_count);
} FunctionVal;

typedef struct {
  RuntimeVal base;
  RuntimeVal **items;
  size_t size;
  size_t capacity;
} ListVal;

typedef struct Entry {
  char *key;
  RuntimeVal *value;
  struct Entry *next;
} Entry;

typedef struct {
  RuntimeVal base;
  Entry **entries;
  size_t size;
  size_t capacity;
} DictVal;

typedef struct {
  RuntimeVal base;
  char **columns;
  size_t column_count;
  DictVal **rows;
  size_t row_count;
  size_t capacity;
} TableVal;

typedef RuntimeVal *(*NativeFn)(Environment *env, RuntimeVal **args,
                                int arg_count);

NilVal *MK_NIL();
BooleanVal *MK_BOOL(unsigned short int b);
NumberVal *MK_NUMBER(double n);
StringVal *MK_STRING(const char *str);
RuntimeVal *create_native_fn(char **params, size_t param_count,
                             RuntimeVal *(*fn)(Environment *env,
                                               RuntimeVal **args,
                                               size_t arg_count));
FunctionVal *MK_FUNCTION(char **params, size_t param_count, Stmt **body,
                         size_t body_count, Environment *env,
                         RuntimeVal *(*builtin_func)(Environment *env,
                                                     RuntimeVal **args,
                                                     size_t arg_count));
ListVal *MK_LIST(size_t capacity);
DictVal *MK_DICT(size_t capacity);
Entry *MK_ENTRY(const char *key, RuntimeVal *value);
TableVal *MK_TABLE(char **columns, size_t column_count);

char *type_to_string(ValueType type);

void free_null(NilVal *val);
void free_boolean(BooleanVal *val);
void free_number(NumberVal *val);

#define MK_NATIVE_FN(params, param_count, fn_ptr) \
  create_native_fn(params, param_count, fn_ptr)

#endif  // VALUE_H
