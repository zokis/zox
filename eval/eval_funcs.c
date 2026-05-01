/* Function calls, variables, assignments. */
#include "eval_internal.h"

RuntimeVal *eval_var_expr(VarDeclaration *var, Environment *env) {
  RuntimeVal *value = evaluate(&(var->value->stmt), env);
  declare_var(env, var->varname, value);
  return value;
}

RuntimeVal *eval_string_literal(StringLiteral *str_literal) {
  return (RuntimeVal *)MK_STRING(str_literal->value);
}

RuntimeVal *eval_assign_var_expr(AssignVar *var, Environment *env) {
  RuntimeVal *value = evaluate(&(var->value->stmt), env);
  assign_var(env, var->varname, value);
  return value;
}

RuntimeVal *eval_assign_list_var_expr(AssignListVar *var, Environment *env) {
  RuntimeVal *value = evaluate(&(var->value->stmt), env);
  RuntimeVal *index_val = evaluate(&(var->index->stmt), env);
  ListVal *list = (ListVal *)lookup_var(env, var->varname);
  int idx = (int)((NumberVal *)index_val)->value;
  release(index_val);
  retain(value);
  release(list->items[idx]);
  list->items[idx] = value;
  return value;
}

RuntimeVal *eval_assign_dict_var_expr(AssignDictVar *var, Environment *env) {
  RuntimeVal *value = evaluate(&(var->value->stmt), env);
  RuntimeVal *key_val = evaluate(&(var->key->stmt), env);
  DictVal *dict = (DictVal *)lookup_var(env, var->varname);
  dict_set_val(dict, runtime_value_to_string(key_val), value);
  release(key_val);
  return value;
}

RuntimeVal *eval_assign_list_expr(AssignListExpr *node, Environment *env) {
  RuntimeVal *value    = evaluate(&(node->value->stmt), env);
  RuntimeVal *list_val = evaluate(&(node->target->stmt), env);
  RuntimeVal *idx_val  = evaluate(&(node->index->stmt), env);
  if (list_val->type != LIST_T) error("Attempted to index-assign a non-list value.\n");
  if (idx_val->type  != NUMBER_T) error("List index must be a number.\n");
  ListVal *list = (ListVal *)list_val;
  int idx = (int)((NumberVal *)idx_val)->value;
  if (idx < 0) idx = list->size + idx;
  if (idx < 0 || idx >= (int)list->size) error("List index out of bounds.\n");
  release(idx_val);
  retain(value);
  release(list->items[idx]);
  list->items[idx] = value;
  release(list_val);
  return value;
}

RuntimeVal *eval_assign_dict_expr(AssignDictExpr *node, Environment *env) {
  RuntimeVal *value    = evaluate(&(node->value->stmt), env);
  RuntimeVal *dict_val = evaluate(&(node->target->stmt), env);
  RuntimeVal *key_val  = evaluate(&(node->key->stmt), env);
  if (dict_val->type != DICT_T) error("Attempted to key-assign a non-dict value.\n");
  char *key = runtime_value_to_string(key_val);
  if (key == NULL) error("Dict key must be convertible to a string.\n");
  dict_set_val((DictVal *)dict_val, key, value);
  release(key_val);
  release(dict_val);
  return value;
}

RuntimeVal *eval_identifier_expr(Identifier *ident, Environment *env) {
  RuntimeVal *val = lookup_var(env, ident->symbol);
  retain(val);
  return val;
}

RuntimeVal *eval_func_def(FuncDef *func_def, Environment *env) {
  FunctionVal *func_val = MK_FUNCTION(func_def->params, func_def->param_count,
                                      func_def->body, func_def->body_count, env, NULL);
  declare_var(env, func_def->name, (RuntimeVal *)func_val);
  return (RuntimeVal *)func_val;
}

/* Args arrive ref+1; func_env keeps them after caller release. */
RuntimeVal *eval_call_expr(CallExpr *call_expr, Environment *env) {
  RuntimeVal *callee = evaluate(&(call_expr->callee->stmt), env);
  if (callee->type != FUNCTION_T) error("Attempted to call a non-function value.\n");
  FunctionVal *func = (FunctionVal *)callee;

  if (call_expr->arg_count != func->param_count) {
    char error_message[100];
    snprintf(error_message, sizeof(error_message),
             "Function expected %ld arguments but got %ld.\n",
             func->param_count, call_expr->arg_count);
    error(error_message);
  }

  if (func->builtin_func != NULL) {
    RuntimeVal **args = malloc_safe(sizeof(RuntimeVal *) * call_expr->arg_count,
                                    "eval_call_expr args");
    for (size_t i = 0; i < call_expr->arg_count; i++) {
      args[i] = evaluate(&(call_expr->arguments[i]->stmt), env);
    }
    RuntimeVal *result = func->builtin_func(env, args, call_expr->arg_count);
    for (size_t i = 0; i < call_expr->arg_count; i++) {
      release(args[i]);
    }
    release(callee);
    free_safe(args);
    return result;
  }

  Environment *func_env = create_environment(func->env, "func_env");
  for (size_t i = 0; i < func->param_count; i++) {
    RuntimeVal *arg_val = evaluate(&(call_expr->arguments[i]->stmt), env);
    declare_var(func_env, func->params[i], arg_val);
    release(arg_val);
  }
  release(callee);

  RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();
  for (size_t i = 0; i < func->body_count; i++) {
    RuntimeVal *tmp = evaluate(func->body[i], func_env);
    if (i < func->body_count - 1) {
      release(tmp);
    } else {
      release(lastEvaluated);
      lastEvaluated = tmp;
    }
    if (cf_signal == CF_RETURN) {
      release(lastEvaluated);
      lastEvaluated = cf_take_return_val();
      cf_signal = CF_NONE;
      break;
    }
    if (cf_signal != CF_NONE) break;
  }
  free_environment(func_env);
  return lastEvaluated;
}

/* Calls builtin or Zox FunctionVal with already-evaluated args. */
RuntimeVal *zox_call_function(FunctionVal *func, Environment *env,
                               RuntimeVal **args, size_t arg_count) {
  if (arg_count != func->param_count) {
    char msg[128];
    snprintf(msg, sizeof(msg),
             "Function expected %zu arguments but got %zu.\n",
             func->param_count, arg_count);
    error(msg);
  }

  if (func->builtin_func)
    return func->builtin_func(env, args, arg_count);

  Environment *func_env = create_environment(func->env, "func_env");
  for (size_t i = 0; i < arg_count; i++) {
    declare_var(func_env, func->params[i], args[i]);
  }

  RuntimeVal *result = (RuntimeVal *)MK_NIL();
  for (size_t i = 0; i < func->body_count; i++) {
    RuntimeVal *tmp = evaluate(func->body[i], func_env);
    if (i < func->body_count - 1) {
      release(tmp);
    } else {
      release(result);
      result = tmp;
    }
    if (cf_signal == CF_RETURN) {
      release(result);
      result = cf_take_return_val();
      cf_signal = CF_NONE;
      break;
    }
    if (cf_signal != CF_NONE) break;
  }
  free_environment(func_env);
  return result;
}
