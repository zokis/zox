/* functional.c -- modulo externo Zox: higher-order functions
   Compile: make buildlib LIB=functional
   Uso:     ~> "./lib/functional.so" { map, filter, reduce, any, all, take, drop, zip_with, pipe }; */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../zox_module.h"

/* helpers: delegam para zox_call_function -- suporta builtins E funcoes Zox puro */
static RuntimeVal *call1(Environment *env, FunctionVal *f, RuntimeVal *arg) {
  RuntimeVal *args[] = {arg};
  return zox_call_function(f, env, args, 1);
}

static RuntimeVal *call2(Environment *env, FunctionVal *f, RuntimeVal *a, RuntimeVal *b) {
  RuntimeVal *args[] = {a, b};
  return zox_call_function(f, env, args, 2);
}

/* -- map(lst, f) --------------------------------------------------------- */
static RuntimeVal *fn_map(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != FUNCTION_T) {
    fprintf(stderr, "functional.map: expects (list, function)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal     *src    = (ListVal *)args[0];
  FunctionVal *f      = (FunctionVal *)args[1];
  ListVal     *result = MK_LIST(src->size);
  for (size_t i = 0; i < src->size; i++) {
    RuntimeVal *mapped = call1(env, f, src->items[i]);
    list_append_val(result, mapped);
    release(mapped);
  }
  return (RuntimeVal *)result;
}

/* -- filter(lst, pred) --------------------------------------------------- */
static RuntimeVal *fn_filter(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != FUNCTION_T) {
    fprintf(stderr, "functional.filter: expects (list, function)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal     *src    = (ListVal *)args[0];
  FunctionVal *pred   = (FunctionVal *)args[1];
  ListVal     *result = MK_LIST(src->size);
  for (size_t i = 0; i < src->size; i++) {
    RuntimeVal *res = call1(env, pred, src->items[i]);
    int truthy = (res->type == BOOLEAN_T && ((BooleanVal *)res)->value) ||
                 (res->type == NUMBER_T  && ((NumberVal  *)res)->value != 0);
    release(res);
    if (truthy) list_append_val(result, src->items[i]);
  }
  return (RuntimeVal *)result;
}

/* -- reduce(lst, f, init) ------------------------------------------------ */
static RuntimeVal *fn_reduce(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 3 || args[0]->type != LIST_T || args[1]->type != FUNCTION_T) {
    fprintf(stderr, "functional.reduce: expects (list, function, init)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal     *src = (ListVal *)args[0];
  FunctionVal *f   = (FunctionVal *)args[1];
  RuntimeVal  *acc = args[2]; retain(acc);
  for (size_t i = 0; i < src->size; i++) {
    RuntimeVal *next = call2(env, f, acc, src->items[i]);
    release(acc);
    acc = next;
  }
  return acc;
}

/* -- any(lst, pred) ------------------------------------------------------ */
static RuntimeVal *fn_any(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != FUNCTION_T) {
    fprintf(stderr, "functional.any: expects (list, function)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal     *src  = (ListVal *)args[0];
  FunctionVal *pred = (FunctionVal *)args[1];
  for (size_t i = 0; i < src->size; i++) {
    RuntimeVal *res = call1(env, pred, src->items[i]);
    int truthy = (res->type == BOOLEAN_T && ((BooleanVal *)res)->value) ||
                 (res->type == NUMBER_T  && ((NumberVal  *)res)->value != 0);
    release(res);
    if (truthy) return (RuntimeVal *)MK_BOOL(1);
  }
  return (RuntimeVal *)MK_BOOL(0);
}

/* -- all(lst, pred) ------------------------------------------------------ */
static RuntimeVal *fn_all(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != FUNCTION_T) {
    fprintf(stderr, "functional.all: expects (list, function)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal     *src  = (ListVal *)args[0];
  FunctionVal *pred = (FunctionVal *)args[1];
  for (size_t i = 0; i < src->size; i++) {
    RuntimeVal *res = call1(env, pred, src->items[i]);
    int truthy = (res->type == BOOLEAN_T && ((BooleanVal *)res)->value) ||
                 (res->type == NUMBER_T  && ((NumberVal  *)res)->value != 0);
    release(res);
    if (!truthy) return (RuntimeVal *)MK_BOOL(0);
  }
  return (RuntimeVal *)MK_BOOL(1);
}

/* -- take(lst, n) -------------------------------------------------------- */
static RuntimeVal *fn_take(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != NUMBER_T) {
    fprintf(stderr, "functional.take: expects (list, number)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal *src    = (ListVal *)args[0];
  size_t   n      = (size_t)((NumberVal *)args[1])->value;
  if (n > src->size) n = src->size;
  ListVal *result = MK_LIST(n);
  for (size_t i = 0; i < n; i++) list_append_val(result, src->items[i]);
  return (RuntimeVal *)result;
}

/* -- drop(lst, n) -------------------------------------------------------- */
static RuntimeVal *fn_drop(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[0]->type != LIST_T || args[1]->type != NUMBER_T) {
    fprintf(stderr, "functional.drop: expects (list, number)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal *src    = (ListVal *)args[0];
  size_t   n      = (size_t)((NumberVal *)args[1])->value;
  if (n > src->size) n = src->size;
  ListVal *result = MK_LIST(src->size - n);
  for (size_t i = n; i < src->size; i++) list_append_val(result, src->items[i]);
  return (RuntimeVal *)result;
}

/* -- zip_with(a, b, f) --------------------------------------------------- */
static RuntimeVal *fn_zip_with(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 3 || args[0]->type != LIST_T ||
      args[1]->type != LIST_T || args[2]->type != FUNCTION_T) {
    fprintf(stderr, "functional.zip_with: expects (list, list, function)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal     *a      = (ListVal *)args[0];
  ListVal     *b      = (ListVal *)args[1];
  FunctionVal *f      = (FunctionVal *)args[2];
  size_t       n      = a->size < b->size ? a->size : b->size;
  ListVal     *result = MK_LIST(n);
  for (size_t i = 0; i < n; i++) {
    RuntimeVal *mapped = call2(env, f, a->items[i], b->items[i]);
    list_append_val(result, mapped);
    release(mapped);
  }
  return (RuntimeVal *)result;
}

/* -- pipe(val, {f1, f2, ...}) -------------------------------------------- */
static RuntimeVal *fn_pipe(Environment *env, RuntimeVal **args, size_t argc) {
  if (argc != 2 || args[1]->type != LIST_T) {
    fprintf(stderr, "functional.pipe: expects (value, list_of_functions)\n");
    return (RuntimeVal *)MK_NIL();
  }
  ListVal    *fns = (ListVal *)args[1];
  RuntimeVal *acc = args[0]; retain(acc);
  for (size_t i = 0; i < fns->size; i++) {
    if (fns->items[i]->type != FUNCTION_T) {
      fprintf(stderr, "functional.pipe: all elements must be functions\n");
      release(acc);
      return (RuntimeVal *)MK_NIL();
    }
    FunctionVal *f    = (FunctionVal *)fns->items[i];
    RuntimeVal  *next = call1(env, f, acc);
    release(acc);
    acc = next;
  }
  return acc;
}

/* -- ponto de entrada ---------------------------------------------------- */
ZOX_MODULE_INIT {
  char *p2[]  = {"a", "b"};
  char *p3[]  = {"a", "b", "f"};
  char *pp[]  = {"value", "fns"};
  char *pr[]  = {"lst", "f", "init"};

  declare_owned(env, "map",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, fn_map));
  declare_owned(env, "filter",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, fn_filter));
  declare_owned(env, "reduce",
    (RuntimeVal *)MK_FUNCTION(pr, 3, NULL, 0, NULL, fn_reduce));
  declare_owned(env, "any",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, fn_any));
  declare_owned(env, "all",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, fn_all));
  declare_owned(env, "take",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, fn_take));
  declare_owned(env, "drop",
    (RuntimeVal *)MK_FUNCTION(p2, 2, NULL, 0, NULL, fn_drop));
  declare_owned(env, "zip_with",
    (RuntimeVal *)MK_FUNCTION(p3, 3, NULL, 0, NULL, fn_zip_with));
  declare_owned(env, "pipe",
    (RuntimeVal *)MK_FUNCTION(pp, 2, NULL, 0, NULL, fn_pipe));
}
