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
  size_t new_capacity = dict->capacity * 2;
  Entry **new_entries = (Entry **)malloc_safe(new_capacity * sizeof(Entry *), "resize_dict");
  for (size_t i = 0; i < new_capacity; i++) new_entries[i] = NULL;
  for (size_t i = 0; i < dict->capacity; i++) {
    Entry *entry = dict->entries[i];
    while (entry != NULL) {
      size_t new_slot = hash(entry->key, new_capacity);
      Entry *next_entry = entry->next;
      entry->next = new_entries[new_slot];
      new_entries[new_slot] = entry;
      entry = next_entry;
    }
  }
  free(dict->entries);
  dict->entries = new_entries;
  dict->capacity = new_capacity;
}

void dict_set_val(DictVal *dict, const char *key, RuntimeVal *value) {
  if ((float)dict->size / dict->capacity > 0.75f) resize_dict(dict);
  size_t slot = hash(key, dict->capacity);
  retain(value);
  Entry *entry = dict->entries[slot];
  if (entry == NULL) {
    dict->entries[slot] = MK_ENTRY(key, value);
    dict->size++;
  } else {
    Entry *prev = NULL;
    while (entry != NULL) {
      if (strcmp(entry->key, key) == 0) {
        release(entry->value);
        entry->value = value;
        return;
      }
      prev = entry;
      entry = entry->next;
    }
    prev->next = MK_ENTRY(key, value);
    dict->size++;
  }
}

char *runtime_value_to_string(RuntimeVal *val) {
  switch (val->type) {
  case NIL_T:     return "nil";
  case BOOLEAN_T: return ((BooleanVal *)val)->value ? "true" : "false";
  case NUMBER_T: {
    char *result = malloc_safe(32, "runtime_value_to_string NUMBER_T");
    sprintf(result, "%f", ((NumberVal *)val)->value);
    return result;
  }
  case STRING_T:  return ((StringVal *)val)->value;
  default:        return NULL;
  }
}

RuntimeVal *eval_dict_literal(DictLiteral *dict_lit, Environment *env) {
  DictVal *dict = MK_DICT(dict_lit->element_count > 0 ? dict_lit->element_count * 2 : 1);
  for (size_t i = 0; i < dict_lit->element_count; i++) {
    RuntimeVal *key   = evaluate(&(dict_lit->keys[i]->stmt), env);
    RuntimeVal *value = evaluate(&(dict_lit->values[i]->stmt), env);
    dict_set_val(dict, runtime_value_to_string(key), value);
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
  char *key = runtime_value_to_string(key_val);
  if (key == NULL) error("Dict key must be convertible to a string.\n");

  size_t slot = hash(key, dict->capacity);
  RuntimeVal *result = NULL;
  for (Entry *entry = dict->entries[slot]; entry != NULL; entry = entry->next) {
    if (strcmp(entry->key, key) == 0) {
      result = entry->value;
      retain(result);
      break;
    }
  }

  release(key_val);
  release(dict_val);
  if (result == NULL) return (RuntimeVal *)MK_NIL();
  return result;
}
