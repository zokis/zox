/* List/dict creation, indexing, slicing, key access. */
#include "eval_internal.h"

void list_append_val(ListVal *list, RuntimeVal *item) {
  retain(item);
  if (list->size >= list->capacity) {
    list->capacity = list->capacity * 2 + 1;
    list->items = zox_realloc_buf(
        ZOX_BUF_LIST_ITEMS, list->items, sizeof(RuntimeVal *) * list->capacity,
        "list_append_val realloc");
  }
  list->items[list->size++] = item;
}

char *dict_key_to_string(RuntimeVal *val) {
  char *result = NULL;
  switch (val->type) {
    case NIL_T: result = zox_strdup_buf(ZOX_BUF_DICT_KEY, "nil"); break;
    case BOOLEAN_T:
      result = zox_strdup_buf(ZOX_BUF_DICT_KEY,
                              ((BooleanVal *)val)->value ? "true" : "false");
      break;
    case NUMBER_T: {
      int needed = snprintf(NULL, 0, "%.17g", ((NumberVal *)val)->value);
      if (needed >= 0) {
        result = zox_alloc_buf(ZOX_BUF_DICT_KEY, (size_t)needed + 1, "dict_key_to_string");
        snprintf(result, (size_t)needed + 1, "%.17g", ((NumberVal *)val)->value);
      }
      break;
    }
    case STRING_T:
      result = zox_strdup_buf(ZOX_BUF_DICT_KEY, ((StringVal *)val)->value);
      break;
    default: break;
  }
  return result;
}

Entry *dict_find_entry(DictVal *dict, const char *key) {
  if (!dict || !key) return NULL;
  size_t index = hash(key, dict->capacity);
  while (dict->entries[index].key != NULL) {
    if (strcmp(dict->entries[index].key, key) == 0) return &dict->entries[index];
    index = (index + 1) % dict->capacity;
  }
  return NULL;
}

RuntimeVal *dict_get_val(DictVal *dict, const char *key) {
  Entry *entry = dict_find_entry(dict, key);
  if (entry == NULL) return NULL;
  retain(entry->value);
  return entry->value;
}

RuntimeVal *eval_list_literal(ListLiteral *list_lit, Environment *env) {
  ListVal *list = MK_LIST(list_lit->element_count > 0 ? list_lit->element_count : 1);
  for (size_t i = 0; i < list_lit->element_count; i++) {
    RuntimeVal *elem = evaluate(&(list_lit->elements[i]->stmt), env);
    list_append_val(list, elem);
    release(elem);
  }
  return (RuntimeVal *)list;
}

void resize_dict(DictVal *dict) {
  size_t old_capacity = dict->capacity;
  Entry *old_entries = dict->entries;
  dict->capacity *= 2;
  dict->entries = (Entry *)zox_calloc_buf(
      ZOX_BUF_DICT_ENTRIES, dict->capacity, sizeof(Entry), "resize_dict");
  for (size_t i = 0; i < dict->capacity; i++) {
    dict->entries[i].key = NULL;
    dict->entries[i].value = NULL;
  }
  for (size_t i = 0; i < old_capacity; i++) {
    if (old_entries[i].key != NULL) {
      size_t index = hash(old_entries[i].key, dict->capacity);
      while (dict->entries[index].key != NULL) {
        index = (index + 1) % dict->capacity;
      }
      dict->entries[index].key = old_entries[i].key;
      dict->entries[index].value = old_entries[i].value;
    }
  }
  zox_free_buf(ZOX_BUF_DICT_ENTRIES, old_entries);
}

void dict_set_val(DictVal *dict, const char *key, RuntimeVal *value) {
  if ((float)dict->size / dict->capacity > 0.75f) resize_dict(dict);
  size_t index = hash(key, dict->capacity);
  while (dict->entries[index].key != NULL) {
    if (strcmp(dict->entries[index].key, key) == 0) {
      release(dict->entries[index].value);
      dict->entries[index].value = value;
      retain(value);
      return;
    }
    index = (index + 1) % dict->capacity;
  }
  dict->entries[index].key = zox_strdup_buf(ZOX_BUF_DICT_KEY, key);
  dict->entries[index].value = value;
  retain(value);
  dict->size++;
}

static char *append_str(char **buf, size_t *len, size_t *cap, const char *s) {
  size_t slen = strlen(s);
  while (*len + slen + 1 > *cap) {
    *cap = (*cap) * 2 + 64;
    *buf = zox_realloc_buf(ZOX_BUF_TEMP, *buf, *cap, "runtime_value_to_string buf");
  }
  memcpy(*buf + *len, s, slen + 1);
  *len += slen;
  return *buf;
}

static char *val_to_repr(RuntimeVal *val);

static void list_to_repr(ListVal *list, char **buf, size_t *len, size_t *cap) {
  append_str(buf, len, cap, "{");
  for (size_t i = 0; i < list->size; i++) {
    if (i > 0) append_str(buf, len, cap, ", ");
    char *item = val_to_repr(list->items[i]);
    append_str(buf, len, cap, item ? item : "nil");
    zox_free_buf(ZOX_BUF_TEMP, item);
  }
  append_str(buf, len, cap, "}");
}

static void dict_to_repr(DictVal *dict, char **buf, size_t *len, size_t *cap) {
  int first = 1;
  append_str(buf, len, cap, "[");
  for (size_t i = 0; i < dict->capacity; i++) {
    if (!dict->entries[i].key) continue;
    if (!first) append_str(buf, len, cap, "; ");
    first = 0;
    append_str(buf, len, cap, "\"");
    append_str(buf, len, cap, dict->entries[i].key);
    append_str(buf, len, cap, "\" -> ");
    char *v = val_to_repr(dict->entries[i].value);
    append_str(buf, len, cap, v ? v : "nil");
    zox_free_buf(ZOX_BUF_TEMP, v);
  }
  append_str(buf, len, cap, "]");
}

static char *type_to_repr(TypeVal *tv) {
  size_t needed = strlen("type<>") + strlen(tv->name) + 1;
  char *buf = zox_alloc_buf(ZOX_BUF_TEMP, needed, "type repr");
  snprintf(buf, needed, "type<%s>", tv->name);
  return buf;
}

static void struct_to_repr(StructVal *sv, char **buf, size_t *len, size_t *cap) {
  append_str(buf, len, cap, sv->type_def->name);
  append_str(buf, len, cap, "(");
  for (size_t i = 0; i < sv->type_def->field_count; i++) {
    if (i > 0) append_str(buf, len, cap, ", ");
    char *v = val_to_repr(sv->values[i]);
    append_str(buf, len, cap, v ? v : "nil");
    zox_free_buf(ZOX_BUF_TEMP, v);
  }
  append_str(buf, len, cap, ")");
}

static char *val_to_repr(RuntimeVal *val) {
  char *prim = dict_key_to_string(val);
  if (prim) return prim;

  char *buf = NULL;
  size_t len = 0, cap = 16;

  switch (val->type) {
    case LIST_T:
      buf = zox_alloc_buf(ZOX_BUF_TEMP, cap, "list repr");
      list_to_repr((ListVal *)val, &buf, &len, &cap);
      break;

    case DICT_T:
      buf = zox_alloc_buf(ZOX_BUF_TEMP, cap, "dict repr");
      dict_to_repr((DictVal *)val, &buf, &len, &cap);
      break;

    case TYPE_T:
      return type_to_repr((TypeVal *)val);

    case STRUCT_T:
      buf = zox_alloc_buf(ZOX_BUF_TEMP, cap, "struct repr");
      struct_to_repr((StructVal *)val, &buf, &len, &cap);
      break;

    default:
      break;
  }

  return buf;
}

char *runtime_value_to_string(RuntimeVal *val) {
  return val_to_repr(val);
}

RuntimeVal *eval_dict_literal(DictLiteral *dict_lit, Environment *env) {
  DictVal *dict = MK_DICT(dict_lit->element_count > 0 ? dict_lit->element_count * 2 : 1);
  for (size_t i = 0; i < dict_lit->element_count; i++) {
    RuntimeVal *key   = evaluate(&(dict_lit->keys[i]->stmt), env);
    RuntimeVal *value = evaluate(&(dict_lit->values[i]->stmt), env);
    char *key_str = runtime_value_to_string(key);
    if (key_str == NULL) error("Dict key must be convertible to a string.\n");
    dict_set_val(dict, key_str, value);
    zox_free_buf(ZOX_BUF_TEMP, key_str);
    release(key);
    release(value);
  }
  return (RuntimeVal *)dict;
}

RuntimeVal *get_list_slice(ListVal *list, int start, int end) {
  if (start < 0) start = list->size + start;
  if (end   < 0) end   = list->size + end;
  start = (start < 0) ? 0 : (start > (int)list->size) ? (int)list->size : start;
  end   = (end   < 0) ? 0 : (end   > (int)list->size) ? (int)list->size : end;
  if (start >= end) return (RuntimeVal *)MK_LIST(0);
  ListVal *slice = MK_LIST(end - start);
  for (int i = start; i < end; i++) list_append_val(slice, list->items[i]);
  return (RuntimeVal *)slice;
}

RuntimeVal *get_string_slice(StringVal *str, int start, int end) {
  size_t size = strlen(str->value);
  if (start < 0) start = size + start;
  if (start < 0 || start >= (int)size) error("String index out of bounds.\n");
  if (end   < 0) end   = size + end;
  if (end   < 0 || end > (int)size)   error("String index out of bounds.\n");
  if (start >= end) return (RuntimeVal *)MK_STRING("");
  char *slice = zox_alloc_buf(ZOX_BUF_TEMP, (size_t)(end - start + 1), "get_string_slice");
  strncpy(slice, str->value + start, end - start);
  slice[end - start] = '\0';
  RuntimeVal *result = (RuntimeVal *)MK_STRING(slice);
  zox_free_buf(ZOX_BUF_TEMP, slice);
  return result;
}

static RuntimeVal *eval_string_index(StringVal *str, int start, ListIndex *list_index, Environment *env) {
  if (!list_index->is_slice) {
    if (start < 0) start = strlen(str->value) + start;
    if (start < 0 || start >= (int)strlen(str->value)) error("String index out of bounds.\n");
    char single_char[2] = {str->value[start], '\0'};
    return (RuntimeVal *)MK_STRING(single_char);
  } else {
    int end = strlen(str->value);
    if (list_index->end != NULL) {
      RuntimeVal *end_val = evaluate(&(list_index->end->stmt), env);
      end = (int)((NumberVal *)end_val)->value;
      release(end_val);
    }
    return get_string_slice(str, start, end);
  }
}

static RuntimeVal *eval_struct_index(StructVal *sv, int start, ListIndex *list_index) {
  if (list_index->is_slice) error("Slicing not supported for structs.");
  if (start < 0 || (size_t)start >= sv->type_def->field_count) error("Struct index out of bounds.");
  RuntimeVal *result = sv->values[start];
  retain(result);
  return result;
}

static RuntimeVal *eval_list_collection_index(ListVal *list, int start, ListIndex *list_index, Environment *env) {
  if (!list_index->is_slice) {
    if (start < 0) start = list->size + start;
    if (start < 0 || start >= (int)list->size) error("List index out of bounds.\n");
    RuntimeVal *result = list->items[start];
    if (!result || result->type == NIL_T) {
      fprintf(stderr, "DEBUG: list[%d] is nil (size=%zu, items=%p, items[%d]=%p)\n",
              start, list->size, (void*)list->items, start, (void*)result);
    }
    retain(result);
    return result;
  } else {
    int end = list->size;
    if (list_index->end != NULL) {
      RuntimeVal *end_val = evaluate(&(list_index->end->stmt), env);
      end = (int)((NumberVal *)end_val)->value;
      release(end_val);
    }
    return get_list_slice(list, start, end);
  }
}

RuntimeVal *eval_list_index(ListIndex *list_index, Environment *env) {
  RuntimeVal *list_val  = evaluate(&(list_index->list->stmt), env);
  RuntimeVal *start_val = evaluate(&(list_index->start->stmt), env);

  if (list_val->type != LIST_T && list_val->type != STRING_T && list_val->type != STRUCT_T) {
    error("Attempted to index a non-collection value.\n");
  }
  if (start_val->type != NUMBER_T) error("Start index must be a number.\n");
  int start = (int)((NumberVal *)start_val)->value;
  release(start_val);

  RuntimeVal *result = NULL;
  switch (list_val->type) {
    case STRING_T: result = eval_string_index((StringVal *)list_val, start, list_index, env); break;
    case STRUCT_T: result = eval_struct_index((StructVal *)list_val, start, list_index); break;
    case LIST_T:   result = eval_list_collection_index((ListVal *)list_val, start, list_index, env); break;
    default: break;
  }

  release(list_val);
  return result;
}

RuntimeVal *eval_dict_key(DictKey *dict_key, Environment *env) {
  RuntimeVal *dict_val = evaluate(&(dict_key->dict->stmt), env);
  if (dict_val->type != DICT_T) error("Attempted to key a non-dict value.\n");
  DictVal *dict = (DictVal *)dict_val;

  RuntimeVal *key_val = evaluate(&(dict_key->key->stmt), env);
  char *key = dict_key_to_string(key_val);
  if (key == NULL) error("Dict key must be convertible to a string.\n");
  RuntimeVal *result = dict_get_val(dict, key);

  release(key_val);
  release(dict_val);
  zox_free_buf(ZOX_BUF_DICT_KEY, key);
  if (result == NULL) return (RuntimeVal *)MK_NIL();
  return result;
}
