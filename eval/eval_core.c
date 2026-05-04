/* Evaluator core: ControlFlow, eval_program, evaluate. */
#include "eval_internal.h"

/* Lightweight break/continue/return propagation without longjmp. */
ControlFlowKind cf_signal    = CF_NONE;
RuntimeVal     *cf_return_val = NULL;

void cf_set_return(RuntimeVal *val) {
  cf_signal = CF_RETURN;
  if (cf_return_val) release(cf_return_val);
  cf_return_val = val;
}

RuntimeVal *cf_take_return_val(void) {
  RuntimeVal *v = cf_return_val ? cf_return_val : (RuntimeVal *)MK_NIL();
  cf_return_val = NULL;
  return v;
}

RuntimeVal *eval_program(Program *program, Environment *env) {
  if (program->body_count == 0) {
    return (RuntimeVal *)MK_NIL();
  }
  RuntimeVal *lastEvaluated = NULL;
  for (size_t i = 0; i < program->body_count; i++) {
    RuntimeVal *result = evaluate(program->body[i], env);
    RuntimeVal *old = lastEvaluated;
    lastEvaluated = result;
    if (old != NULL) {
      release(old);
    }
  }
  return lastEvaluated;
}

static RuntimeVal *eval_binary_node(BinaryExpr *binop, Environment *env) {
  RuntimeVal *lhs = evaluate(&(binop->left->stmt), env);
  RuntimeVal *rhs = evaluate(&(binop->right->stmt), env);
  RuntimeVal *result = eval_binary_expr_evaluated(lhs, rhs, binop->operator);
  release(lhs);
  release(rhs);
  return result;
}

static RuntimeVal *eval_return_node(ReturnStmt *ret, Environment *env) {
  RuntimeVal *val = ret->value ? evaluate(&(ret->value->stmt), env) : (RuntimeVal *)MK_NIL();
  cf_set_return(val);
  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *eval_result_return_node(ReturnStmt *ret, Environment *env, const char *key) {
  RuntimeVal *val = ret->value ? evaluate(&(ret->value->stmt), env) : (RuntimeVal *)MK_NIL();
  DictVal *res = MK_DICT(1);
  dict_set_val(res, key, val);
  cf_set_return((RuntimeVal *)res);
  release(val);
  return (RuntimeVal *)MK_NIL();
}

static RuntimeVal *eval_unwrap_node(UnwrapExpr *u, Environment *env) {
  RuntimeVal *val = evaluate(&(u->expr->stmt), env);
  if (val->type != DICT_T) error("Unwrap operator !? expects a result dictionary {ok: v} or {err: v}.");
  DictVal *d = (DictVal *)val;
  RuntimeVal *err_val = dict_get_val(d, "err");
  if (err_val) {
    /* Propagate early return: the value IS the result dict. */
    cf_set_return(val);
    release(err_val);
    return (RuntimeVal *)MK_NIL();
  }
  RuntimeVal *ok_val = dict_get_val(d, "ok");
  if (!ok_val) error("Unwrap operator !? expects a result dictionary with 'ok' or 'err' key.");
  retain(ok_val);
  release(val);
  return ok_val; /* caller takes ownership */
}

RuntimeVal *evaluate(Stmt *astNode, Environment *env) {
  RuntimeVal *result = NULL;

  switch (astNode->kind) {
  case ProgramAst:
    result = eval_program((Program *)astNode, env);
    break;
  case BooleanLiteralAst:
    result = (RuntimeVal *)MK_BOOL(((BooleanLiteral *)astNode)->value);
    break;
  case NilAst:
    result = (RuntimeVal *)MK_NIL();
    break;
  case NumericLiteralAst:
    result = (RuntimeVal *)MK_NUMBER(((NumericLiteral *)astNode)->value);
    break;
  case IdentifierAst:
    result = eval_identifier_expr((Identifier *)astNode, env);
    break;
  case BinaryExprAst:
    result = eval_binary_node((BinaryExpr *)astNode, env);
    break;
  case VarDeclarationAst:
    result = eval_var_expr((VarDeclaration *)astNode, env);
    break;
  case AssignVarAst:
    result = eval_assign_var_expr((AssignVar *)astNode, env);
    break;
  case IfAst:
    result = eval_if_expr((IfExpr *)astNode, env);
    break;
  case WhileAst:
    result = eval_while_expr((WhileExpr *)astNode, env);
    break;
  case MatchAst:
    result = eval_match_expr((MatchExpr *)astNode, env);
    break;
  case ForAst:
    result = eval_for_expr((ForExpr *)astNode, env);
    break;
  case ArenaBlockAst:
    result = eval_arena_block((ArenaBlockExpr *)astNode, env);
    break;
  case StringLiteralAst:
    result = eval_string_literal((StringLiteral *)astNode);
    break;
  case FuncDefAst:
    result = eval_func_def((FuncDef *)astNode, env);
    break;
  case TypeDeclarationAst:
    result = eval_type_declaration((TypeDeclaration *)astNode, env);
    break;
  case CallExprAst:
    result = eval_call_expr((CallExpr *)astNode, env);
    break;
  case MemberExprAst:
    result = eval_member_expr((MemberExpr *)astNode, env);
    break;
  case AssignMemberExprAst:
    result = eval_assign_member_expr((AssignMemberExpr *)astNode, env);
    break;
  case ListLiteralAst:
    result = eval_list_literal((ListLiteral *)astNode, env);
    break;
  case DictLiteralAst:
    result = eval_dict_literal((DictLiteral *)astNode, env);
    break;
  case ListIndexAst:
    result = eval_list_index((ListIndex *)astNode, env);
    break;
  case DictKeyAst:
    result = eval_dict_key((DictKey *)astNode, env);
    break;
  case AssignListVarAst:
    result = eval_assign_list_var_expr((AssignListVar *)astNode, env);
    break;
  case AssignDictVarAst:
    result = eval_assign_dict_var_expr((AssignDictVar *)astNode, env);
    break;
  case ImportAst:
    result = eval_import_stmt((ImportStmt *)astNode, env);
    break;
  case AssignListExprAst:
    result = eval_assign_list_expr((AssignListExpr *)astNode, env);
    break;
  case AssignDictExprAst:
    result = eval_assign_dict_expr((AssignDictExpr *)astNode, env);
    break;
  case UnaryExprAst:
    result = eval_unary_expr((UnaryExpr *)astNode, env);
    break;
  case BreakAst:
    cf_signal = CF_BREAK;
    result = (RuntimeVal *)MK_NIL();
    break;
  case ContinueAst:
    cf_signal = CF_CONTINUE;
    result = (RuntimeVal *)MK_NIL();
    break;
  case ReturnAst:
    result = eval_return_node((ReturnStmt *)astNode, env);
    break;
  case ReturnSuccessAst:
    result = eval_result_return_node((ReturnStmt *)astNode, env, "ok");
    break;
  case ReturnErrorAst:
    result = eval_result_return_node((ReturnStmt *)astNode, env, "err");
    break;
  case UnwrapAst:
    result = eval_unwrap_node((UnwrapExpr *)astNode, env);
    break;
  default: error("This AST Node has not yet been setup for interpretation.\n");
  }
  return result;
}

#include "../zox_alloc.h"

static RuntimeVal *promote_list_val(ListVal *old_list) {
  ListVal *new_list = MK_LIST(old_list->size);
  for (size_t i = 0; i < old_list->size; i++) {
    RuntimeVal *promoted_item = promote_val(old_list->items[i]);
    new_list->items[i] = promoted_item;
    retain(promoted_item);
    if (promoted_item != old_list->items[i]) release(promoted_item);
  }
  new_list->size = old_list->size;
  return (RuntimeVal *)new_list;
}

static RuntimeVal *promote_dict_val(DictVal *old_dict) {
  DictVal *new_dict = MK_DICT(old_dict->capacity);
  for (size_t i = 0; i < old_dict->capacity; i++) {
    if (old_dict->entries[i].key) {
      RuntimeVal *promoted_val = promote_val(old_dict->entries[i].value);
      dict_set_val(new_dict, old_dict->entries[i].key, promoted_val);
      if (promoted_val != old_dict->entries[i].value) release(promoted_val);
    }
  }
  return (RuntimeVal *)new_dict;
}

static RuntimeVal *promote_function_val(FunctionVal *fv) {
  return (RuntimeVal *)MK_FUNCTION(
      fv->params, fv->param_count, fv->body, fv->body_count, fv->env, fv->builtin_func);
}

static RuntimeVal *promote_struct_val(StructVal *old_struct) {
  size_t count = old_struct->type_def->field_count;
  RuntimeVal **new_values = malloc_safe(sizeof(RuntimeVal *) * count, "promote_struct_val");
  for (size_t i = 0; i < count; i++) {
    new_values[i] = promote_val(old_struct->values[i]);
    retain(new_values[i]);
    if (new_values[i] != old_struct->values[i]) release(new_values[i]);
  }
  return (RuntimeVal *)MK_STRUCT(old_struct->type_def, new_values);
}

static RuntimeVal *promote_type_val(TypeVal *old_type) {
  /* Type definitions are usually permanent, but if declared in arena: */
  char **new_fields = malloc_safe(sizeof(char *) * old_type->field_count, "promote_type_val fields");
  for (size_t i = 0; i < old_type->field_count; i++) {
    new_fields[i] = strdup(old_type->fields[i]);
  }
  return (RuntimeVal *)MK_TYPE(old_type->name, new_fields, old_type->field_count);
}

static int val_needs_promotion(RuntimeVal *val) {
  if (!val || val->ref_count == STATIC_REF) return 0;
  if (zox_arena_owns(val)) return 1;

  switch (val->type) {
    case LIST_T: {
      ListVal *list = (ListVal *)val;
      for (size_t i = 0; i < list->size; i++) {
        if (val_needs_promotion(list->items[i])) return 1;
      }
      return 0;
    }
    case DICT_T: {
      DictVal *dict = (DictVal *)val;
      for (size_t i = 0; i < dict->capacity; i++) {
        if (dict->entries[i].key != NULL &&
            val_needs_promotion(dict->entries[i].value)) {
          return 1;
        }
      }
      return 0;
    }
    case STRUCT_T: {
      StructVal *sv = (StructVal *)val;
      if (val_needs_promotion((RuntimeVal *)sv->type_def)) return 1;
      for (size_t i = 0; i < sv->type_def->field_count; i++) {
        if (val_needs_promotion(sv->values[i])) return 1;
      }
      return 0;
    }
    default:
      return 0;
  }
}

RuntimeVal *promote_val(RuntimeVal *val) {
  RuntimeVal *promoted = val;

  if (!val || val->ref_count == STATIC_REF) {
    return promoted;
  }
  if (!val_needs_promotion(val)) {
    return promoted;
  }

  switch (val->type) {
    case NUMBER_T:
      promoted = (RuntimeVal *)MK_NUMBER(((NumberVal *)val)->value);
      break;
    case STRING_T:
      promoted = (RuntimeVal *)MK_STRING(((StringVal *)val)->value);
      break;
    case LIST_T:
      promoted = promote_list_val((ListVal *)val);
      break;
    case DICT_T:
      promoted = promote_dict_val((DictVal *)val);
      break;
    case FUNCTION_T:
      promoted = promote_function_val((FunctionVal *)val);
      break;
    case STRUCT_T:
      promoted = promote_struct_val((StructVal *)val);
      break;
    case TYPE_T:
      promoted = promote_type_val((TypeVal *)val);
      break;
    default:
      break;
  }

  return promoted;
}
