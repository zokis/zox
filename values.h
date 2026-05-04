#ifndef VALUE_H
#define VALUE_H

#include "ast.h"
#include <stddef.h>

/* Forward declaration to avoid circular dependency with env.h */
typedef struct Environment Environment;

/* ref_count = -1 -> static singleton, retain/release no-op. */
#define STATIC_REF (-1)

typedef enum {
  NIL_T,
  NUMBER_T,
  BOOLEAN_T,
  STRING_T,
  FUNCTION_T,
  LIST_T,
  DICT_T,
  TYPE_T,
  STRUCT_T,
  MODULE_T
} ValueType;

typedef struct RuntimeVal {
  ValueType type;
  int ref_count;
} RuntimeVal;

typedef struct {
  RuntimeVal base;
} NilVal;

typedef struct {
  RuntimeVal base;
  double value;
} NumberVal;

typedef struct {
  RuntimeVal base;
  int value;
} BooleanVal;

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

typedef struct {
  char *key;
  RuntimeVal *value;
} Entry;

typedef struct {
  RuntimeVal base;
  Entry *entries;
  size_t size;
  size_t capacity;
} DictVal;

typedef struct {
  RuntimeVal base;
  char *name;
  char **fields;
  size_t field_count;
} TypeVal;

typedef struct {
  RuntimeVal base;
  TypeVal *type_def;
  RuntimeVal **values;
} StructVal;

typedef struct {
  RuntimeVal base;
  Environment *env;
} ModuleVal;

typedef RuntimeVal *(*NativeFn)(Environment *env, RuntimeVal **args,
                                int arg_count);

NilVal *MK_NIL();
BooleanVal *MK_BOOL(int value);
NumberVal *MK_NUMBER(double n);
StringVal *MK_STRING(const char *str);
FunctionVal *MK_FUNCTION(char **params, size_t param_count, Stmt **body,
                         size_t body_count, Environment *env,
                         RuntimeVal *(*builtin_func)(Environment *env,
                                                     RuntimeVal **args,
                                                     size_t arg_count));
RuntimeVal *create_native_fn(char **params, size_t param_count,
                             RuntimeVal *(*fn)(Environment *env,
                                               RuntimeVal **args,
                                               size_t arg_count));

ListVal *MK_LIST(size_t capacity);
DictVal *MK_DICT(size_t capacity);
TypeVal *MK_TYPE(const char *name, char **fields, size_t field_count);
StructVal *MK_STRUCT(TypeVal *type_def, RuntimeVal **values);
ModuleVal *MK_MODULE(Environment *env);

RuntimeVal *promote_val(RuntimeVal *val);
char *dict_key_to_string(RuntimeVal *val);
Entry *dict_find_entry(DictVal *dict, const char *key);
RuntimeVal *dict_get_val(DictVal *dict, const char *key);

const char *type_to_string(ValueType type);

/* Reference counting. */
void retain(RuntimeVal *val);
void release(RuntimeVal *val);

void free_null(NilVal *val);
void free_boolean(BooleanVal *val);
void free_number(NumberVal *val);

#define MK_NATIVE_FN(params, param_count, fn_ptr) \
  create_native_fn(params, param_count, fn_ptr)

#endif  // VALUE_H
