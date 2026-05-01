#include "values.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_safe.h"

/* ref_count = -1 -> static singleton, retain/release no-op. */
#define STATIC_REF (-1)

static NilVal     _nil_singleton   = { .base = { NIL_T,     STATIC_REF } };
static BooleanVal _true_singleton  = { .base = { BOOLEAN_T, STATIC_REF }, .value = 1 };
static BooleanVal _false_singleton = { .base = { BOOLEAN_T, STATIC_REF }, .value = 0 };
static NumberVal  _num_singletons[256];
static int        _singletons_initialized = 0;

static void init_singletons(void) {
  if (_singletons_initialized) return;
  for (int i = 0; i < 256; i++) {
    _num_singletons[i].base.type      = NUMBER_T;
    _num_singletons[i].base.ref_count = STATIC_REF;
    _num_singletons[i].value          = (double)i;
  }
  _singletons_initialized = 1;
}

static void free_runtime_val(RuntimeVal *val) {
  if (!val) return;
  switch (val->type) {
    case NIL_T:
    case NUMBER_T:
    case BOOLEAN_T:
      free_safe(val);
      break;
    case STRING_T: {
      StringVal *sv = (StringVal *)val;
      free_safe(sv->value);
      free_safe(sv);
      break;
    }
    case LIST_T: {
      ListVal *lv = (ListVal *)val;
      for (size_t i = 0; i < lv->size; i++) release(lv->items[i]);
      free_safe(lv->items);
      free_safe(lv);
      break;
    }
    case DICT_T: {
      DictVal *dv = (DictVal *)val;
      for (size_t i = 0; i < dv->capacity; i++) {
        Entry *e = dv->entries[i];
        while (e) {
          Entry *next = e->next;
          free_safe(e->key);
          release(e->value);
          free_safe(e);
          e = next;
        }
      }
      free_safe(dv->entries);
      free_safe(dv);
      break;
    }
    case FUNCTION_T: {
      /* params/body owned by AST. */
      FunctionVal *fv = (FunctionVal *)val;
      if (fv->env != NULL) release_env(fv->env);
      free_safe(val);
      break;
    }
  }
}

void retain(RuntimeVal *val) {
  if (!val || val->ref_count == STATIC_REF) return;
  val->ref_count++;
}

void release(RuntimeVal *val) {
  if (!val || val->ref_count == STATIC_REF) return;
  val->ref_count--;
  if (val->ref_count <= 0) free_runtime_val(val);
}

NilVal *MK_NIL() {
  init_singletons();
  return &_nil_singleton;
}

BooleanVal *MK_BOOL(unsigned short int b) {
  init_singletons();
  return b ? &_true_singleton : &_false_singleton;
}

NumberVal *MK_NUMBER(double n) {
  init_singletons();
  if (n >= 0.0 && n <= 255.0 && n == (double)(int)n) {
    return &_num_singletons[(int)n];
  }
  NumberVal *val = (NumberVal *)malloc_safe(sizeof(NumberVal), "NumberVal");
  val->base.type      = NUMBER_T;
  val->base.ref_count = 1;
  val->value          = n;
  return val;
}

StringVal *MK_STRING(const char *str) {
  StringVal *val = (StringVal *)malloc_safe(sizeof(StringVal), "StringVal");
  val->base.type      = STRING_T;
  val->base.ref_count = 1;
  val->value          = strdup(str);
  return val;
}

ListVal *MK_LIST(size_t capacity) {
  if (capacity == 0) capacity = 1;
  ListVal *list = (ListVal *)malloc_safe(sizeof(ListVal), "ListVal");
  list->base.type      = LIST_T;
  list->base.ref_count = 1;
  list->items = (RuntimeVal **)malloc_safe(sizeof(RuntimeVal *) * capacity, "ListVal items");
  list->size     = 0;
  list->capacity = capacity;
  return list;
}

Entry *MK_ENTRY(const char *key, RuntimeVal *value) {
  Entry *entry  = malloc_safe(sizeof(Entry), "Entry");
  entry->key    = strdup(key);
  entry->value  = value;
  entry->next   = NULL;
  return entry;
}

DictVal *MK_DICT(size_t capacity) {
  if (capacity == 0) capacity = 1;
  DictVal *dict = (DictVal *)malloc_safe(sizeof(DictVal), "DictVal");
  dict->base.type      = DICT_T;
  dict->base.ref_count = 1;
  dict->entries = (Entry **)malloc_safe(sizeof(Entry *) * capacity, "DictVal items");
  for (size_t i = 0; i < capacity; ++i) dict->entries[i] = NULL;
  dict->size     = 0;
  dict->capacity = capacity;
  return dict;
}


FunctionVal *MK_FUNCTION(char **params, size_t param_count, Stmt **body,
                         size_t body_count, Environment *env,
                         RuntimeVal *(*builtin_func)(Environment *env,
                                                     RuntimeVal **args,
                                                     size_t arg_count)) {
  FunctionVal *val = (FunctionVal *)malloc_safe(sizeof(FunctionVal), "FunctionVal");
  val->base.type      = FUNCTION_T;
  val->base.ref_count = 1;
  val->params         = params;
  val->param_count    = param_count;
  val->body           = body;
  val->body_count     = body_count;
  val->env            = env;
  if (env != NULL) retain_env(env);  /* closure keeps definition env */
  val->builtin_func   = builtin_func;
  return val;
}

RuntimeVal *create_native_fn(char **params, size_t param_count,
                             RuntimeVal *(*fn)(Environment *env,
                                               RuntimeVal **args,
                                               size_t arg_count)) {
  FunctionVal *func_val = malloc_safe(sizeof(FunctionVal), "create_native_fn");
  func_val->base.type      = FUNCTION_T;
  func_val->base.ref_count = 1;
  func_val->params         = params;
  func_val->param_count    = param_count;
  func_val->body           = NULL;
  func_val->body_count     = 0;
  func_val->env            = NULL;
  func_val->builtin_func   = fn;
  return (RuntimeVal *)func_val;
}

char *type_to_string(ValueType type) {
  switch (type) {
    case NIL_T:      return "nil";
    case NUMBER_T:   return "number";
    case BOOLEAN_T:  return "boolean";
    case STRING_T:   return "string";
    case FUNCTION_T: return "function";
    case LIST_T:     return "list";
    case DICT_T:     return "dict";
    default:         return "unknown";
  }
}
