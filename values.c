#include "values.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_safe.h"
#include "zox_alloc.h"

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
    case BOOLEAN_T:
    case MODULE_T:
      free_safe(val);
      break;
    case NUMBER_T:
      zox_free_obj(ZOX_ALLOC_NUMBER, val);
      break;
    case STRING_T: {
      StringVal *sv = (StringVal *)val;
      free_safe(sv->value);
      zox_free_obj(ZOX_ALLOC_STRING, sv);
      break;
    }
    case LIST_T: {
      ListVal *lv = (ListVal *)val;
      for (size_t i = 0; i < lv->size; i++) release(lv->items[i]);
      free_safe(lv->items);
      zox_free_obj(ZOX_ALLOC_LIST, lv);
      break;
    }
    case DICT_T: {
      DictVal *d = (DictVal *)val;
      for (size_t i = 0; i < d->capacity; i++) {
        if (d->entries[i].key) {
          free_safe(d->entries[i].key);
          release(d->entries[i].value);
        }
      }
      free_safe(d->entries);
      zox_free_obj(ZOX_ALLOC_DICT, d);
      break;
    }
    case TYPE_T: {
      TypeVal *tv = (TypeVal *)val;
      free_safe(tv->name);
      for (size_t i = 0; i < tv->field_count; i++) free_safe(tv->fields[i]);
      free_safe(tv->fields);
      zox_free_obj(ZOX_ALLOC_DICT, tv);
      break;
    }
    case STRUCT_T: {
      StructVal *sv = (StructVal *)val;
      size_t field_count = sv->type_def->field_count;
      for (size_t i = 0; i < field_count; i++) {
        release(sv->values[i]);
      }
      free_safe(sv->values);
      release((RuntimeVal *)sv->type_def);
      zox_free_obj(ZOX_ALLOC_LIST, sv);
      break;
    }
    case FUNCTION_T: {

      /* params/body owned by AST. */
      FunctionVal *fv = (FunctionVal *)val;
      if (fv->env != NULL) release_env(fv->env);
      zox_free_obj(ZOX_ALLOC_FUNCTION, fv);
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
  NumberVal *val = zox_alloc_obj(ZOX_ALLOC_NUMBER, sizeof(NumberVal), "NumberVal");
  val->base.type      = NUMBER_T;
  val->base.ref_count = 1;
  val->value          = n;
  return val;
}

StringVal *MK_STRING(const char *str) {
  StringVal *val = zox_alloc_obj(ZOX_ALLOC_STRING, sizeof(StringVal), "StringVal");
  val->base.type      = STRING_T;
  val->base.ref_count = 1;
  val->value          = strdup(str);
  return val;
}

ListVal *MK_LIST(size_t capacity) {
  if (capacity < 8) capacity = 8;
  ListVal *list = zox_alloc_obj(ZOX_ALLOC_LIST, sizeof(ListVal), "ListVal");
  list->base.type      = LIST_T;
  list->base.ref_count = 1;
  list->items = (RuntimeVal **)malloc_safe(sizeof(RuntimeVal *) * capacity, "ListVal items");
  list->size     = 0;
  list->capacity = capacity;
  return list;
}

DictVal *MK_DICT(size_t capacity) {
  if (capacity < 8) capacity = 8;
  DictVal *dict = zox_alloc_obj(ZOX_ALLOC_DICT, sizeof(DictVal), "DictVal");
  dict->base.type      = DICT_T;
  dict->base.ref_count = 1;
  dict->entries = (Entry *)malloc_safe(sizeof(Entry) * capacity, "DictVal items");
  for (size_t i = 0; i < capacity; ++i) {
    dict->entries[i].key = NULL;
    dict->entries[i].value = NULL;
  }
  dict->size     = 0;
  dict->capacity = capacity;
  return dict;
}


FunctionVal *MK_FUNCTION(char **params, size_t param_count, Stmt **body,
                         size_t body_count, Environment *env,
                         RuntimeVal *(*builtin_func)(Environment *env,
                                                     RuntimeVal **args,
                                                     size_t arg_count)) {
  FunctionVal *val = zox_alloc_obj(ZOX_ALLOC_FUNCTION, sizeof(FunctionVal), "FunctionVal");
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
  FunctionVal *func_val = zox_alloc_obj(ZOX_ALLOC_FUNCTION,
                                        sizeof(FunctionVal),
                                        "create_native_fn");
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
    case TYPE_T:     return "type_def";
    case STRUCT_T:   return "struct";
    default:         return "unknown";
  }
}

TypeVal *MK_TYPE(const char *name, char **fields, size_t field_count) {
  TypeVal *tv = (TypeVal *)zox_alloc_obj(ZOX_ALLOC_DICT, sizeof(TypeVal), "TypeVal");
  tv->base.type = TYPE_T;
  tv->base.ref_count = 1;
  tv->name = strdup(name);
  tv->fields = fields;
  tv->field_count = field_count;
  return tv;
}

StructVal *MK_STRUCT(TypeVal *type_def, RuntimeVal **values) {
  StructVal *sv = (StructVal *)zox_alloc_obj(ZOX_ALLOC_LIST, sizeof(StructVal), "StructVal");
  sv->base.type = STRUCT_T;
  sv->base.ref_count = 1;
  sv->type_def = type_def;
  retain((RuntimeVal *)type_def);
  sv->values = values;
  return sv;
}
