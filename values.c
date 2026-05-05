#include "values.h"
#include "env.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "zox_alloc.h"

static NilVal _nil_singleton = { .base = { NIL_T,     STATIC_REF } };
static BooleanVal _true_singleton  = { .base = { BOOLEAN_T, STATIC_REF }, .value = 1 };
static BooleanVal _false_singleton = { .base = { BOOLEAN_T, STATIC_REF }, .value = 0 };

static void init_singletons(void) {
  /* Done via static init above. */
}

static int string_uses_inline_storage(StringVal *sv) {
  return sv->value == sv->inline_buf;
}

static int list_uses_inline_storage(ListVal *lv) {
  return lv->items == lv->inline_items;
}

static int dict_uses_inline_storage(DictVal *dv) {
  return dv->entries == dv->inline_entries;
}

static int struct_uses_inline_storage(StructVal *sv) {
  return sv->values == sv->inline_values;
}

void list_reserve(ListVal *list, size_t capacity) {
  if (capacity <= list->capacity) return;

  RuntimeVal **new_items = NULL;
  if (list_uses_inline_storage(list)) {
    new_items = (RuntimeVal **)zox_calloc_buf(
        ZOX_BUF_LIST_ITEMS, capacity, sizeof(RuntimeVal *), "ListVal grow");
    if (list->size > 0) {
      memcpy(new_items, list->inline_items, list->size * sizeof(RuntimeVal *));
    }
  } else {
    new_items = (RuntimeVal **)zox_realloc_buf(
        ZOX_BUF_LIST_ITEMS, list->items, sizeof(RuntimeVal *) * capacity,
        "ListVal grow");
  }

  list->items = new_items;
  list->capacity = capacity;
}

void dict_reserve(DictVal *dict, size_t capacity) {
  if (capacity <= dict->capacity) return;

  Entry *new_entries = (Entry *)zox_calloc_buf(
      ZOX_BUF_DICT_ENTRIES, capacity, sizeof(Entry), "DictVal grow");

  for (size_t i = 0; i < dict->capacity; i++) {
    if (dict->entries[i].key != NULL) {
      size_t index = hash(dict->entries[i].key, capacity);
      while (new_entries[index].key != NULL) {
        index = (index + 1) % capacity;
      }
      new_entries[index].key = dict->entries[i].key;
      new_entries[index].value = dict->entries[i].value;
    }
  }

  if (dict->entries && !dict_uses_inline_storage(dict)) {
    zox_free_buf(ZOX_BUF_DICT_ENTRIES, dict->entries);
  }

  dict->entries = new_entries;
  dict->capacity = capacity;
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
      zox_free_buf(ZOX_BUF_MISC, mv);
      break;
    }
    case NUMBER_T:
      zox_free_obj(ZOX_ALLOC_NUMBER, val);
      break;
    case STRING_T: {
      StringVal *sv = (StringVal *)val;
      if (sv->value && !string_uses_inline_storage(sv)) {
        zox_free_buf(ZOX_BUF_STRING, sv->value);
      }
      sv->value = NULL;
      sv->len = 0;
      zox_free_obj(ZOX_ALLOC_STRING, sv);
      break;
    }
    case LIST_T: {
      ListVal *lv = (ListVal *)val;
      for (size_t i = 0; i < lv->size; i++) release(lv->items[i]);
      if (lv->items && !list_uses_inline_storage(lv)) {
        zox_free_buf(ZOX_BUF_LIST_ITEMS, lv->items);
      }
      lv->items = NULL;
      zox_free_obj(ZOX_ALLOC_LIST, lv);
      break;
    }
    case DICT_T: {
      DictVal *d = (DictVal *)val;
      if (d->entries) {
        for (size_t i = 0; i < d->capacity; i++) {
          if (d->entries[i].key) {
            zox_free_buf(ZOX_BUF_DICT_KEY, d->entries[i].key);
            release(d->entries[i].value);
          }
        }
        if (!dict_uses_inline_storage(d)) {
          zox_free_buf(ZOX_BUF_DICT_ENTRIES, d->entries);
        }
        d->entries = NULL;
      }
      zox_free_obj(ZOX_ALLOC_DICT, d);
      break;
    }
    case TYPE_T: {
      TypeVal *tv = (TypeVal *)val;
      zox_free_buf(ZOX_BUF_STRING, tv->name);
      tv->name = NULL;
      for (size_t i = 0; i < tv->field_count; i++) {
        zox_free_buf(ZOX_BUF_STRING, tv->fields[i]);
      }
      zox_free_buf(ZOX_BUF_TYPE_FIELDS, tv->fields);
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
      if (sv->values && !struct_uses_inline_storage(sv)) {
        zox_free_buf(ZOX_BUF_STRUCT_VALUES, sv->values);
      }
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
  size_t len = strlen(str);
  val->base.type      = STRING_T;
  val->base.ref_count = 1;
  val->len            = len;
  if (len < sizeof(val->inline_buf)) {
    memcpy(val->inline_buf, str, len + 1);
    val->value = val->inline_buf;
  } else {
    val->value = zox_strdup_buf(ZOX_BUF_STRING, str);
  }
  return val;
}

ModuleVal *MK_MODULE(Environment *env) {
  ModuleVal *val = (ModuleVal *)zox_alloc_buf(ZOX_BUF_MISC, sizeof(ModuleVal), "ModuleVal");
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
  if (lv->items && !list_uses_inline_storage(lv)) {
    zox_free_buf(ZOX_BUF_LIST_ITEMS, lv->items);
  }
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
  list->size           = 0;
  list->capacity       = capacity;
  if (capacity <= sizeof(list->inline_items) / sizeof(list->inline_items[0])) {
    memset(list->inline_items, 0, sizeof(list->inline_items));
    list->items = list->inline_items;
  } else {
    list->items = (RuntimeVal **)zox_calloc_buf(
        ZOX_BUF_LIST_ITEMS, capacity, sizeof(RuntimeVal *), "ListVal items");
  }
  return list;
}

static void dict_pool_cleanup(void *ptr) {
  DictVal *dv = (DictVal *)ptr;
  if (dv->entries && !dict_uses_inline_storage(dv)) {
    zox_free_buf(ZOX_BUF_DICT_ENTRIES, dv->entries);
  }
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
  dict->size     = 0;
  dict->capacity = capacity;
  if (capacity <= sizeof(dict->inline_entries) / sizeof(dict->inline_entries[0])) {
    memset(dict->inline_entries, 0, sizeof(dict->inline_entries));
    dict->entries = dict->inline_entries;
  } else {
    dict->entries = (Entry *)zox_calloc_buf(
        ZOX_BUF_DICT_ENTRIES, capacity, sizeof(Entry), "DictVal items");
  }
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
  if (tv->fields) zox_free_buf(ZOX_BUF_TYPE_FIELDS, tv->fields);
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
  tv->name = zox_strdup_buf(ZOX_BUF_STRING, name);
  tv->fields = fields;
  tv->field_count = field_count;
  return tv;
}

static void struct_pool_cleanup(void *ptr) {
  StructVal *sv = (StructVal *)ptr;
  if (sv->values && !struct_uses_inline_storage(sv)) {
    zox_free_buf(ZOX_BUF_STRUCT_VALUES, sv->values);
  }
}

StructVal *MK_STRUCT(TypeVal *type_def, RuntimeVal **values) {
  static int cleanup_registered = 0;
  if (!cleanup_registered) {
    zox_alloc_set_cleanup_fn(ZOX_ALLOC_STRUCT, struct_pool_cleanup);
    cleanup_registered = 1;
  }
  size_t field_count = type_def->field_count;
  StructVal *sv = (StructVal *)zox_alloc_obj(ZOX_ALLOC_STRUCT, sizeof(StructVal), "StructVal");
  sv->base.type = STRUCT_T;
  sv->base.ref_count = 1;
  sv->type_def = type_def;
  retain((RuntimeVal *)type_def);
  if (field_count <= sizeof(sv->inline_values) / sizeof(sv->inline_values[0])) {
    memset(sv->inline_values, 0, sizeof(sv->inline_values));
    for (size_t i = 0; i < field_count; i++) {
      sv->inline_values[i] = values[i];
    }
    sv->values = sv->inline_values;
  } else {
    sv->values = values;
  }
  return sv;
}

StructVal *MK_STRUCT_COPY_VALUES(TypeVal *type_def, RuntimeVal **values) {
  size_t field_count = type_def->field_count;
  RuntimeVal **owned_values = values;

  if (field_count > sizeof(((StructVal *)0)->inline_values) / sizeof(RuntimeVal *)) {
    owned_values = zox_alloc_buf(
        ZOX_BUF_STRUCT_VALUES, sizeof(RuntimeVal *) * field_count, "StructVal values");
    memcpy(owned_values, values, sizeof(RuntimeVal *) * field_count);
  }

  return MK_STRUCT(type_def, owned_values);
}
