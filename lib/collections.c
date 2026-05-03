/* External Zox module: list/dict operations.
   Compile: make buildlib LIB=collections
   Use:     ~> "./lib/collections.so" { range, zip, flatten, unique, chunk, count, group_by }; */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../zox_module.h"

static RuntimeVal *col_range(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc < 2 || args[0]->type != NUMBER_T || args[1]->type != NUMBER_T) {
    fprintf(stderr, "collections.range: expects range(start, end [, step])\n");
    return (RuntimeVal *)MK_NIL();
  }
  double start = ((NumberVal *)args[0])->value;
  double end   = ((NumberVal *)args[1])->value;
  double step  = (argc >= 3 && args[2]->type == NUMBER_T)
                   ? ((NumberVal *)args[2])->value : 1.0;
  if (step == 0) { fprintf(stderr, "collections.range: step cannot be zero\n"); return (RuntimeVal *)MK_NIL(); }

  double diff = end - start;
  if ((step > 0 && diff <= 0) || (step < 0 && diff >= 0)) return (RuntimeVal *)MK_LIST(0);

  size_t cap = (size_t)(diff / step) + 1;
  ListVal *list = MK_LIST(cap);
  for (double v = start; (step > 0 ? v < end : v > end); v += step) {
    RuntimeVal *n = (RuntimeVal *)MK_NUMBER(v);
    list_append_val(list, n);
    release(n);
  }
  return (RuntimeVal *)list;
}

static RuntimeVal *col_zip(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != LIST_T) {
    fprintf(stderr, "collections.zip: expects two lists\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal *a = (ListVal *)args[0];
  ListVal *b = (ListVal *)args[1];
  size_t   n = a->size < b->size ? a->size : b->size;
  ListVal *result = MK_LIST(n);
  for (size_t i = 0; i < n; i++) {
    ListVal *pair = MK_LIST(2);
    list_append_val(pair, a->items[i]);
    list_append_val(pair, b->items[i]);
    list_append_val(result, (RuntimeVal *)pair);
    release((RuntimeVal *)pair);
  }
  return (RuntimeVal *)result;
}

static RuntimeVal *col_flatten(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1 || args[0]->type != LIST_T) {
    fprintf(stderr, "collections.flatten: expects one list\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal *src    = (ListVal *)args[0];
  size_t   total  = 0;
  for (size_t i = 0; i < src->size; i++) {
    if (src->items[i]->type == LIST_T) total += ((ListVal *)src->items[i])->size;
    else total++;
  }

  ListVal *result = MK_LIST(total);
  for (size_t i = 0; i < src->size; i++) {
    if (src->items[i]->type == LIST_T) {
      ListVal *inner = (ListVal *)src->items[i];
      for (size_t j = 0; j < inner->size; j++) {
        list_append_val(result, inner->items[j]);
      }
    } else {
      list_append_val(result, src->items[i]);
    }
  }
  return (RuntimeVal *)result;
}

static int rval_eq(RuntimeVal *a, RuntimeVal *b) {
  if (a == b) return 1;
  if (a->type != b->type) return 0;
  if (a->type == NIL_T) return 1;
  if (a->type == NUMBER_T) return ((NumberVal *)a)->value == ((NumberVal *)b)->value;
  if (a->type == BOOLEAN_T) return ((BooleanVal *)a)->value == ((BooleanVal *)b)->value;
  if (a->type == STRING_T) return strcmp(((StringVal *)a)->value, ((StringVal *)b)->value) == 0;
  return 0;
}

static RuntimeVal *col_unique(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 1 || args[0]->type != LIST_T) {
    fprintf(stderr, "collections.unique: expects one list\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal *src    = (ListVal *)args[0];
  ListVal *result = MK_LIST(src->size);
  for (size_t i = 0; i < src->size; i++) {
    int found = 0;
    for (size_t j = 0; j < result->size; j++) {
      if (rval_eq(src->items[i], result->items[j])) { found = 1; break; }
    }
    if (!found) list_append_val(result, src->items[i]);
  }
  return (RuntimeVal *)result;
}

static RuntimeVal *col_chunk(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != NUMBER_T) {
    fprintf(stderr, "collections.chunk: expects (list, number)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal *src  = (ListVal *)args[0];
  size_t   n    = (size_t)((NumberVal *)args[1])->value;
  if (n == 0) { fprintf(stderr, "collections.chunk: size must be > 0\n"); return (RuntimeVal *)MK_NIL(); }
  size_t   chunks = (src->size + n - 1) / n;
  ListVal *result = MK_LIST(chunks);
  for (size_t i = 0; i < src->size; i += n) {
    size_t   end   = i + n < src->size ? i + n : src->size;
    ListVal *chunk = MK_LIST(end - i);
    for (size_t j = i; j < end; j++) list_append_val(chunk, src->items[j]);
    list_append_val(result, (RuntimeVal *)chunk);
    release((RuntimeVal *)chunk);
  }
  return (RuntimeVal *)result;
}

static RuntimeVal *col_count(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T) {
    fprintf(stderr, "collections.count: expects (list, value)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal *src = (ListVal *)args[0];
  double   n   = 0;
  for (size_t i = 0; i < src->size; i++)
    if (rval_eq(src->items[i], args[1])) n++;
  return (RuntimeVal *)MK_NUMBER(n);
}

static void group_key_to_string(RuntimeVal *key_val, char *buf, size_t size) {
  if (key_val->type == STRING_T)
    snprintf(buf, size, "%s", ((StringVal *)key_val)->value);
  else if (key_val->type == NUMBER_T)
    snprintf(buf, size, "%g", ((NumberVal *)key_val)->value);
  else if (key_val->type == BOOLEAN_T)
    snprintf(buf, size, "%s", ((BooleanVal *)key_val)->value ? "true" : "false");
  else
    snprintf(buf, size, "nil");
}

static RuntimeVal *col_group_by(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != FUNCTION_T) {
    fprintf(stderr, "collections.group_by: expects (list, function)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal    *src  = (ListVal *)args[0];
  FunctionVal *f   = (FunctionVal *)args[1];
  size_t      initial_cap = src->size > 8 ? src->size / 2 : 8;
  DictVal    *dict = MK_DICT(initial_cap);

  for (size_t i = 0; i < src->size; i++) {
    RuntimeVal *item   = src->items[i];
    RuntimeVal *fargs[] = {item};
    RuntimeVal *key_val = zox_call_function(f, env, fargs, 1);
    
    char key_buf[64];
    const char *key_ptr;
    if (key_val->type == STRING_T) {
      key_ptr = ((StringVal *)key_val)->value;
    } else {
      group_key_to_string(key_val, key_buf, sizeof(key_buf));
      key_ptr = key_buf;
    }

    Entry *entry = dict_find_entry(dict, key_ptr);
    if (!entry) {
      ListVal *bucket = MK_LIST(4);
      dict_set_val(dict, key_ptr, (RuntimeVal *)bucket);
      list_append_val(bucket, item);
      release((RuntimeVal *)bucket);
    } else {
      list_append_val((ListVal *)entry->value, item);
    }
    release(key_val);
  }
  return (RuntimeVal *)dict;
}

ZOX_MODULE_INIT {
  char *p1[]  = {"value"};
  char *p2[]  = {"a", "b"};
  char *p3[]  = {"start", "end", "step"};

  declare_owned(env, "range",
    (RuntimeVal *)MK_FUNCTION(p3, 3, NULL, 0, NULL, col_range));
  declare_owned(env, "zip",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, col_zip));
  declare_owned(env, "flatten",
    (RuntimeVal *)MK_FUNCTION(p1, 1, NULL, 0, NULL, col_flatten));
  declare_owned(env, "unique",
    (RuntimeVal *)MK_FUNCTION(p1, 1, NULL, 0, NULL, col_unique));
  declare_owned(env, "chunk",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, col_chunk));
  declare_owned(env, "count",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, col_count));
  declare_owned(env, "group_by",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, col_group_by));
}
