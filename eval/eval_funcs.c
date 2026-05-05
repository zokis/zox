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
  RuntimeVal *target = lookup_var(env, var->varname);
  
  if (index_val->type != NUMBER_T) {
    release(value);
    release(index_val);
    error("Index must be a number.\n");
  }
  int idx = (int)((NumberVal *)index_val)->value;
  release(index_val);

  if (target->type == STRUCT_T) {
    StructVal *sv = (StructVal *)target;
    if (idx < 0 || (size_t)idx >= sv->type_def->field_count) {
      release(value);
      error("Struct index out of bounds.");
    }
    release(sv->values[idx]);
    sv->values[idx] = value;
    retain(value);
  } else if (target->type == LIST_T) {
    ListVal *list = (ListVal *)target;
    if (idx < 0 || (size_t)idx >= list->size) {
      release(value);
      error("List index out of bounds.");
    }
    release(list->items[idx]);
    list->items[idx] = value;
    retain(value);
  } else {
    release(value);
    error("Target is not indexable.\n");
  }
  return value;
}

RuntimeVal *eval_assign_dict_var_expr(AssignDictVar *var, Environment *env) {
  RuntimeVal *value = evaluate(&(var->value->stmt), env);
  RuntimeVal *key_val = evaluate(&(var->key->stmt), env);
  RuntimeVal *target = lookup_var(env, var->varname);
  
  if (target->type != DICT_T) {
    release(value);
    release(key_val);
    error("Target is not a dictionary.\n");
  }
  
  DictVal *dict = (DictVal *)target;
  char *key = dict_key_to_string(key_val);
  if (key == NULL) {
    release(value);
    release(key_val);
    error("Dict key must be convertible to a string.\n");
  }
  
  dict_set_val(dict, key, value);
  zox_free_buf(ZOX_BUF_DICT_KEY, key);
  release(key_val);
  return value;
}

RuntimeVal *eval_assign_list_expr(AssignListExpr *node, Environment *env) {
  RuntimeVal *value    = evaluate(&(node->value->stmt), env);
  RuntimeVal *list_val = evaluate(&(node->target->stmt), env);
  RuntimeVal *idx_val  = evaluate(&(node->index->stmt), env);
  if (list_val->type != LIST_T && list_val->type != STRUCT_T) error("Attempted to index-assign a non-collection value.\n");
  if (idx_val->type  != NUMBER_T) error("Index must be a number.\n");
  int idx = (int)((NumberVal *)idx_val)->value;
  release(idx_val);

  if (list_val->type == LIST_T) {
    ListVal *list = (ListVal *)list_val;
    if (idx < 0) idx = list->size + idx;
    if (idx < 0 || idx >= (int)list->size) error("List index out of bounds.\n");
    retain(value);
    release(list->items[idx]);
    list->items[idx] = value;
  } else {
    StructVal *sv = (StructVal *)list_val;
    if (idx < 0 || (size_t)idx >= sv->type_def->field_count) error("Struct index out of bounds.");
    retain(value);
    release(sv->values[idx]);
    sv->values[idx] = value;
  }
  release(list_val);
  return value;
}

RuntimeVal *eval_assign_dict_expr(AssignDictExpr *node, Environment *env) {
  RuntimeVal *value    = evaluate(&(node->value->stmt), env);
  RuntimeVal *dict_val = evaluate(&(node->target->stmt), env);
  RuntimeVal *key_val  = evaluate(&(node->key->stmt), env);
  if (dict_val->type != DICT_T) error("Attempted to key-assign a non-dict value.\n");
  char *key = dict_key_to_string(key_val);
  if (key == NULL) error("Dict key must be convertible to a string.\n");
  dict_set_val((DictVal *)dict_val, key, value);
  zox_free_buf(ZOX_BUF_DICT_KEY, key);
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
RuntimeVal *eval_type_declaration(TypeDeclaration *type_decl, Environment *env) {
  /* Copy field names to avoid ownership issues if type_decl is freed */
  char **fields = zox_alloc_buf(
      ZOX_BUF_TYPE_FIELDS, sizeof(char *) * type_decl->field_count, "eval_type fields");
  for (size_t i = 0; i < type_decl->field_count; i++) {
    fields[i] = zox_strdup_buf(ZOX_BUF_STRING, type_decl->fields[i]);
  }
  TypeVal *tv = MK_TYPE(type_decl->name, fields, type_decl->field_count);
  declare_var(env, tv->name, (RuntimeVal *)tv);
  return (RuntimeVal *)tv;
}

static RuntimeVal **eval_call_args(CallExpr *call_expr, Environment *env) {
  RuntimeVal **args = zox_alloc_buf(
      ZOX_BUF_TEMP, sizeof(RuntimeVal *) * call_expr->arg_count, "eval_call_expr args");
  for (size_t i = 0; i < call_expr->arg_count; i++) {
    args[i] = evaluate(&(call_expr->arguments[i]->stmt), env);
  }
  return args;
}

static void release_call_args(RuntimeVal **args, size_t arg_count) {
  for (size_t i = 0; i < arg_count; i++) {
    release(args[i]);
  }
  zox_free_buf(ZOX_BUF_TEMP, args);
}

static RuntimeVal *eval_function_body(FunctionVal *func, Environment *func_env) {
  RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();

  for (size_t i = 0; i < func->body_count; i++) {
    RuntimeVal *tmp = evaluate(func->body[i], func_env);
    if (i < func->body_count - 1) {
      release(tmp);
    } else {
      RuntimeVal *old = lastEvaluated;
      lastEvaluated = tmp;
      release(old);
    }
    if (cf_signal == CF_RETURN) {
      RuntimeVal *old = lastEvaluated;
      lastEvaluated = cf_take_return_val();
      release(old);
      cf_signal = CF_NONE;
      break;
    }
    if (cf_signal != CF_NONE) {
      break;
    }
  }

  return lastEvaluated;
}

static void bind_call_args(Environment *func_env, char **params,
                           RuntimeVal **args, size_t arg_count, int retain_args) {
  for (size_t i = 0; i < arg_count; i++) {
    declare_var(func_env, params[i], args[i]);
    if (!retain_args) {
      release(args[i]);
    }
  }
}

RuntimeVal *eval_call_expr(CallExpr *call_expr, Environment *env) {
  RuntimeVal *callee = evaluate(&(call_expr->callee->stmt), env);
  
  if (callee->type == TYPE_T) {
    TypeVal *type_def = (TypeVal *)callee;
    if (call_expr->arg_count != type_def->field_count) {
      error("Struct constructor argument count mismatch.");
    }
    RuntimeVal *inline_values[4];
    RuntimeVal **values = type_def->field_count <= 4
        ? inline_values
        : zox_alloc_buf(
            ZOX_BUF_STRUCT_VALUES, sizeof(RuntimeVal *) * type_def->field_count, "struct values");
    for (size_t i = 0; i < type_def->field_count; i++) {
      values[i] = evaluate(&(call_expr->arguments[i]->stmt), env);
    }
    StructVal *sv = MK_STRUCT_COPY_VALUES(type_def, values);
    if (type_def->field_count > 4) {
      zox_free_buf(ZOX_BUF_STRUCT_VALUES, values);
    }
    release(callee);
    return (RuntimeVal *)sv;
  }

  if (callee->type != FUNCTION_T) error("Attempted to call a non-function value.\n");
  FunctionVal *func = (FunctionVal *)callee;

  if (call_expr->arg_count != func->param_count) {
    char error_message[100];
    snprintf(error_message, sizeof(error_message),
             "Function expected %zu arguments but got %zu.\n",
             func->param_count, call_expr->arg_count);
    error(error_message);
  }

  if (func->builtin_func != NULL) {
    RuntimeVal **args = eval_call_args(call_expr, env);
    RuntimeVal *result = func->builtin_func(env, args, call_expr->arg_count);
    release(callee);
    release_call_args(args, call_expr->arg_count);
    return result;
  }

  Environment *func_env = create_environment(func->env, "func_env");
  RuntimeVal **args = eval_call_args(call_expr, env);
  bind_call_args(func_env, func->params, args, func->param_count, 1);
  release_call_args(args, call_expr->arg_count);
  release(callee);

  RuntimeVal *lastEvaluated = eval_function_body(func, func_env);
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
  bind_call_args(func_env, func->params, args, arg_count, 1);
  RuntimeVal *result = eval_function_body(func, func_env);
  free_environment(func_env);
  return result;
}
