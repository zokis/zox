#include "values.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_safe.h"

NilVal *MK_NIL() {
  NilVal *val = (NilVal *)malloc_safe(sizeof(NilVal), "NilVal");
  val->base.type = NIL_T;
  return val;
}

BooleanVal *MK_BOOL(unsigned short int b) {
  BooleanVal *val = (BooleanVal *)malloc_safe(sizeof(BooleanVal), "BooleanVal");
  val->base.type = BOOLEAN_T;
  val->value = b;
  return val;
}

NumberVal *MK_NUMBER(double n) {
  NumberVal *val = (NumberVal *)malloc_safe(sizeof(NumberVal), "NumberVal");
  val->base.type = NUMBER_T;
  val->value = n;
  return val;
}

ListVal *MK_LIST(size_t capacity) {
  ListVal *list = (ListVal *)malloc_safe(sizeof(ListVal), "ListVal");
  list->base.type = LIST_T;
  list->items = (RuntimeVal **)malloc_safe(sizeof(RuntimeVal *) * capacity,
                                           "ListVal items");
  list->size = 0;
  list->capacity = capacity;
  return list;
}

Entry *MK_ENTRY(const char *key, RuntimeVal *value) {
  Entry *entry = malloc_safe(sizeof(Entry), "Entry");
  entry->key = strdup(key);
  entry->value = value;
  entry->next = NULL;
  return entry;
}

DictVal *MK_DICT(size_t capacity) {
  DictVal *dict = (DictVal *)malloc_safe(sizeof(DictVal), "DictVal");
  dict->base.type = DICT_T;
  dict->entries =
      (Entry **)malloc_safe(sizeof(Entry *) * capacity, "DictVal items");
  for (size_t i = 0; i < capacity; ++i) {
    dict->entries[i] = NULL;
  }
  dict->size = 0;
  dict->capacity = capacity;
  return dict;
}

char *type_to_string(ValueType type) {
  switch (type) {
    case NIL_T:
      return "nil";
    case NUMBER_T:
      return "number";
    case BOOLEAN_T:
      return "boolean";
    case STRING_T:
      return "string";
    case FUNCTION_T:
      return "function";
    case LIST_T:
      return "list";
    case TABLE_T:
      return "table";
    case DICT_T:
      return "dict";
    default:
      return "unknown";
  }
}

StringVal *MK_STRING(const char *str) {
  StringVal *val = (StringVal *)malloc_safe(sizeof(StringVal), "StringVal");
  val->base.type = STRING_T;
  val->value = strdup(str);
  return val;
}

FunctionVal *MK_FUNCTION(char **params, size_t param_count, Stmt **body,
                         size_t body_count, Environment *env,
                         RuntimeVal *(*builtin_func)(Environment *env,
                                                     RuntimeVal **args,
                                                     size_t arg_count)) {
  FunctionVal *val =
      (FunctionVal *)malloc_safe(sizeof(FunctionVal), "FunctionVal");
  val->base.type = FUNCTION_T;
  val->params = params;
  val->param_count = param_count;
  val->body = body;
  val->body_count = body_count;
  val->env = env;
  val->builtin_func = builtin_func;
  return val;
}

TableVal *MK_TABLE(char **columns, size_t column_count) {
  TableVal *table = (TableVal *)malloc_safe(sizeof(TableVal), "TableVal");
  table->base.type = TABLE_T;
  table->columns =
      (char **)malloc_safe(sizeof(char *) * column_count, "TableVal columns");
  for (size_t i = 0; i < column_count; i++) {
    table->columns[i] = strdup(columns[i]);
  }
  table->column_count = column_count;
  table->rows = NULL;
  table->row_count = 0;
  table->capacity = 0;
  return table;
}

RuntimeVal *create_native_fn(char **params, size_t param_count,
                             RuntimeVal *(*fn)(Environment *env,
                                               RuntimeVal **args,
                                               size_t arg_count)) {
  FunctionVal *func_val = malloc_safe(sizeof(FunctionVal), "create_native_fn");
  func_val->base.type = FUNCTION_T;
  func_val->params = params;
  func_val->param_count = param_count;
  func_val->body = NULL;
  func_val->body_count = 0;
  func_val->env = NULL;
  func_val->builtin_func = fn;
  return (RuntimeVal *)func_val;
}