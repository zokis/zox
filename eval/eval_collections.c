/* List/dict creation, indexing, slicing, key access. */
#include "eval_internal.h"

void list_append_val(ListVal *list, RuntimeVal *item) {
  retain(item);
  if (list->size >= list->capacity) {
    list->capacity = list->capacity * 2 + 1;
    list->items = realloc_safe(list->items, sizeof(RuntimeVal *) * list->capacity,
                               "list_append_val realloc");
  }
  list->items[list->size++] = item;
}

char *dict_key_to_string(RuntimeVal *val) {
  switch (val->type) {
    case NIL_T:     return strdup("nil");
    case BOOLEAN_T: return strdup(((BooleanVal *)val)->value ? "true" : "false");
    case NUMBER_T: {
      int needed = snprintf(NULL, 0, "%.17g", ((NumberVal *)val)->value);
      if (needed < 0) return NULL;
      char *result = malloc_safe((size_t)needed + 1, "dict_key_to_string");
      snprintf(result, (size_t)needed + 1, "%.17g", ((NumberVal *)val)->value);
      return result;
    }
    case STRING_T:  return strdup(((StringVal *)val)->value);
    default:        return NULL;
  }
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
  dict->entries = (Entry *)malloc_safe(sizeof(Entry) * dict->capacity, "resize_dict");
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
  free(old_entries);
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
  dict->entries[index].key = strdup(key);
  dict->entries[index].value = value;
  retain(value);
  dict->size++;
}

char *runtime_value_to_string(RuntimeVal *val) {
  return dict_key_to_string(val);
}

RuntimeVal *eval_dict_literal(DictLiteral *dict_lit, Environment *env) {
  DictVal *dict = MK_DICT(dict_lit->element_count > 0 ? dict_lit->element_count * 2 : 1);
  for (size_t i = 0; i < dict_lit->element_count; i++) {
    RuntimeVal *key   = evaluate(&(dict_lit->keys[i]->stmt), env);
    RuntimeVal *value = evaluate(&(dict_lit->values[i]->stmt), env);
    char *key_str = runtime_value_to_string(key);
    if (key_str == NULL) error("Dict key must be convertible to a string.\n");
    dict_set_val(dict, key_str, value);
    free_safe(key_str);
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
  char *slice = malloc_safe(end - start + 1, "get_string_slice");
  strncpy(slice, str->value + start, end - start);
  slice[end - start] = '\0';
  RuntimeVal *result = (RuntimeVal *)MK_STRING(slice);
  free_safe(slice);
  return result;
}

RuntimeVal *eval_list_index(ListIndex *list_index, Environment *env) {
  RuntimeVal *list_val  = evaluate(&(list_index->list->stmt), env);
  RuntimeVal *start_val = evaluate(&(list_index->start->stmt), env);

  if (list_val->type != LIST_T && list_val->type != STRING_T) {
    error("Attempted to index a non-list value.\n");
  }
  if (start_val->type != NUMBER_T) error("Start index must be a number.\n");
  int start = (int)((NumberVal *)start_val)->value;
  release(start_val);

  RuntimeVal *result = NULL;

  if (list_val->type == STRING_T) {
    StringVal *str = (StringVal *)list_val;
    if (!list_index->is_slice) {
      if (start < 0) start = strlen(str->value) + start;
      if (start < 0 || start >= (int)strlen(str->value)) error("String index out of bounds.\n");
      char single_char[2] = {str->value[start], '\0'};
      result = (RuntimeVal *)MK_STRING(single_char);
    } else {
      int end = strlen(str->value);
      if (list_index->end != NULL) {
        RuntimeVal *end_val = evaluate(&(list_index->end->stmt), env);
        end = (int)((NumberVal *)end_val)->value;
        release(end_val);
      }
      result = get_string_slice(str, start, end);
    }
  } else {
    ListVal *list = (ListVal *)list_val;
    if (!list_index->is_slice) {
      if (start < 0) start = list->size + start;
      if (start < 0 || start >= (int)list->size) error("List index out of bounds.\n");
      result = list->items[start];
      retain(result);
    } else {
      int end = list->size;
      if (list_index->end != NULL) {
        RuntimeVal *end_val = evaluate(&(list_index->end->stmt), env);
        end = (int)((NumberVal *)end_val)->value;
        release(end_val);
      }
      result = get_list_slice(list, start, end);
    }
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
  free_safe(key);
  if (result == NULL) return (RuntimeVal *)MK_NIL();
  return result;
}
