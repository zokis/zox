#include "values.h"
#include "env.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_safe.h"
#include "zox_alloc.h"

static NilVal _nil_singleton = { .base = { NIL_T,     STATIC_REF } };
static BooleanVal _true_singleton  = { .base = { BOOLEAN_T, STATIC_REF }, .value = 1 };
static BooleanVal _false_singleton = { .base = { BOOLEAN_T, STATIC_REF }, .value = 0 };

static void init_singletons(void) {
  /* Done via static init above. */
}

static void free_runtime_val(RuntimeVal *val) {
  if (!val) return;
  switch (val->type) {
    case NIL_T:
    case BOOLEAN_T:
      /* Singletons are never freed. */
      break;
    case MODULE_T: {
      ModuleVal *mv = (ModuleVal *)val;
      if (mv->env) release_env(mv->env);
      free_safe(mv);
      break;
    }
    case NUMBER_T:
      zox_free_obj(ZOX_ALLOC_NUMBER, val);
      break;
    case STRING_T: {
      StringVal *sv = (StringVal *)val;
      free_safe(sv->value);
      sv->value = NULL;
      zox_free_obj(ZOX_ALLOC_STRING, sv);
      break;
    }
    case LIST_T: {
      ListVal *lv = (ListVal *)val;
      for (size_t i = 0; i < lv->size; i++) release(lv->items[i]);
      free_safe(lv->items);
      lv->items = NULL;
      zox_free_obj(ZOX_ALLOC_LIST, lv);
      break;
    }
    case DICT_T: {
      DictVal *d = (DictVal *)val;
      if (d->entries) {
        for (size_t i = 0; i < d->capacity; i++) {
          if (d->entries[i].key) {
            free_safe(d->entries[i].key);
            release(d->entries[i].value);
          }
        }
        free_safe(d->entries);
        d->entries = NULL;
      }
      zox_free_obj(ZOX_ALLOC_DICT, d);
      break;
    }
    case TYPE_T: {
      TypeVal *tv = (TypeVal *)val;
      free_safe(tv->name);
      tv->name = NULL;
      for (size_t i = 0; i < tv->field_count; i++) free_safe(tv->fields[i]);
      free_safe(tv->fields);
      tv->fields = NULL;
      zox_free_obj(ZOX_ALLOC_TYPE, tv);
      break;
    }
    case STRUCT_T: {
      StructVal *sv = (StructVal *)val;
      size_t field_count = sv->type_def->field_count;
      for (size_t i = 0; i < field_count; i++) {
        release(sv->values[i]);
      }
      free_safe(sv->values);
      sv->values = NULL;
      release((RuntimeVal *)sv->type_def);
      zox_free_obj(ZOX_ALLOC_STRUCT, sv);
      break;
    }
    case FUNCTION_T: {
      /* params/body owned by AST. */
      FunctionVal *fv = (FunctionVal *)val;
      if (fv->env != NULL) release_env(fv->env);
      zox_free_obj(ZOX_ALLOC_FUNCTION, fv);
      break;
    }
    default: break;
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

BooleanVal *MK_BOOL(int value) {
  init_singletons();
  return value ? &_true_singleton : &_false_singleton;
}

NumberVal *MK_NUMBER(double value) {
  NumberVal *val = (NumberVal *)zox_alloc_obj(ZOX_ALLOC_NUMBER, sizeof(NumberVal), "NumberVal");
  val->base.type      = NUMBER_T;
  val->base.ref_count = 1;
  val->value          = value;
  return val;
}

StringVal *MK_STRING(const char *str) {
  StringVal *val = (StringVal *)zox_alloc_obj(ZOX_ALLOC_STRING, sizeof(StringVal), "StringVal");
  val->base.type      = STRING_T;
  val->base.ref_count = 1;
  val->value          = strdup(str);
  return val;
}

ModuleVal *MK_MODULE(Environment *env) {
  ModuleVal *val = (ModuleVal *)malloc_safe(sizeof(ModuleVal), "ModuleVal");
  val->base.type      = MODULE_T;
  val->base.ref_count = 1;
  val->env            = env;
  if (env) retain_env(env);
  return val;
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
  return (RuntimeVal *)MK_FUNCTION(params, param_count, NULL, 0, NULL, fn);
}

static void list_pool_cleanup(void *ptr) {
  ListVal *lv = (ListVal *)ptr;
  if (lv->items) free_safe(lv->items);
}

ListVal *MK_LIST(size_t capacity) {
  static int cleanup_registered = 0;
  if (!cleanup_registered) {
    zox_alloc_set_cleanup_fn(ZOX_ALLOC_LIST, list_pool_cleanup);
    cleanup_registered = 1;
  }
  if (capacity < 4) capacity = 4;
  ListVal *list = (ListVal *)zox_alloc_obj(ZOX_ALLOC_LIST, sizeof(ListVal), "ListVal");
  list->base.type      = LIST_T;
  list->base.ref_count = 1;
  list->items          = (RuntimeVal **)calloc_safe(capacity, sizeof(RuntimeVal *), "ListVal items");
  list->size           = 0;
  list->capacity       = capacity;
  return list;
}

static void dict_pool_cleanup(void *ptr) {
  DictVal *dv = (DictVal *)ptr;
  if (dv->entries) free_safe(dv->entries);
}

DictVal *MK_DICT(size_t capacity) {
  static int cleanup_registered = 0;
  if (!cleanup_registered) {
    zox_alloc_set_cleanup_fn(ZOX_ALLOC_DICT, dict_pool_cleanup);
    cleanup_registered = 1;
  }
  if (capacity < 8) capacity = 8;
  DictVal *dict = zox_alloc_obj(ZOX_ALLOC_DICT, sizeof(DictVal), "DictVal");
  dict->base.type      = DICT_T;
  dict->base.ref_count = 1;
  dict->entries = (Entry *)calloc_safe(capacity, sizeof(Entry), "DictVal items");
  dict->size     = 0;
  dict->capacity = capacity;
  return dict;
}

const char *type_to_string(ValueType type) {
  const char *name = "unknown";
  switch (type) {
    case NIL_T:      name = "nil"; break;
    case NUMBER_T:   name = "number"; break;
    case BOOLEAN_T:  name = "boolean"; break;
    case STRING_T:   name = "string"; break;
    case FUNCTION_T: name = "function"; break;
    case LIST_T:     name = "list"; break;
    case DICT_T:     name = "dict"; break;
    case TYPE_T:     name = "type_def"; break;
    case STRUCT_T:   name = "struct"; break;
    default:         break;
  }

  return name;
}

static void type_pool_cleanup(void *ptr) {
  TypeVal *tv = (TypeVal *)ptr;
  if (tv->fields) free_safe(tv->fields);
}

TypeVal *MK_TYPE(const char *name, char **fields, size_t field_count) {
  static int cleanup_registered = 0;
  if (!cleanup_registered) {
    zox_alloc_set_cleanup_fn(ZOX_ALLOC_TYPE, type_pool_cleanup);
    cleanup_registered = 1;
  }
  TypeVal *tv = (TypeVal *)zox_alloc_obj(ZOX_ALLOC_TYPE, sizeof(TypeVal), "TypeVal");
  tv->base.type = TYPE_T;
  tv->base.ref_count = 1;
  tv->name = strdup(name);
  tv->fields = fields;
  tv->field_count = field_count;
  return tv;
}

static void struct_pool_cleanup(void *ptr) {
  StructVal *sv = (StructVal *)ptr;
  if (sv->values) free_safe(sv->values);
}

StructVal *MK_STRUCT(TypeVal *type_def, RuntimeVal **values) {
  static int cleanup_registered = 0;
  if (!cleanup_registered) {
    zox_alloc_set_cleanup_fn(ZOX_ALLOC_STRUCT, struct_pool_cleanup);
    cleanup_registered = 1;
  }
  StructVal *sv = (StructVal *)zox_alloc_obj(ZOX_ALLOC_STRUCT, sizeof(StructVal), "StructVal");
  sv->base.type = STRUCT_T;
  sv->base.ref_count = 1;
  sv->type_def = type_def;
  retain((RuntimeVal *)type_def);
  sv->values = values;
  return sv;
}
