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

RuntimeVal *evaluate(Stmt *astNode, Environment *env) {
  switch (astNode->kind) {
  case ProgramAst:         return eval_program((Program *)astNode, env);
  case BooleanLiteralAst:  return (RuntimeVal *)MK_BOOL(((BooleanLiteral *)astNode)->value);
  case NilAst:             return (RuntimeVal *)MK_NIL();
  case NumericLiteralAst:  return (RuntimeVal *)MK_NUMBER(((NumericLiteral *)astNode)->value);
  case IdentifierAst:      return eval_identifier_expr((Identifier *)astNode, env);
  case BinaryExprAst: {
    BinaryExpr *binop = (BinaryExpr *)astNode;
    RuntimeVal *lhs = evaluate(&(binop->left->stmt), env);
    RuntimeVal *rhs = evaluate(&(binop->right->stmt), env);
    RuntimeVal *result = eval_binary_expr_evaluated(lhs, rhs, binop->operator);
    release(lhs);
    release(rhs);
    return result;
  }
  case VarDeclarationAst:  return eval_var_expr((VarDeclaration *)astNode, env);
  case AssignVarAst:       return eval_assign_var_expr((AssignVar *)astNode, env);
  case IfAst:              return eval_if_expr((IfExpr *)astNode, env);
  case WhileAst:           return eval_while_expr((WhileExpr *)astNode, env);
  case MatchAst:           return eval_match_expr((MatchExpr *)astNode, env);
  case ForAst:             return eval_for_expr((ForExpr *)astNode, env);
  case ArenaBlockAst:      return eval_arena_block((ArenaBlockExpr *)astNode, env);
  case StringLiteralAst:   return eval_string_literal((StringLiteral *)astNode);
  case FuncDefAst:         return eval_func_def((FuncDef *)astNode, env);
  case CallExprAst:        return eval_call_expr((CallExpr *)astNode, env);
  case ListLiteralAst:     return eval_list_literal((ListLiteral *)astNode, env);
  case DictLiteralAst:     return eval_dict_literal((DictLiteral *)astNode, env);
  case ListIndexAst:       return eval_list_index((ListIndex *)astNode, env);
  case DictKeyAst:         return eval_dict_key((DictKey *)astNode, env);
  case AssignListVarAst:   return eval_assign_list_var_expr((AssignListVar *)astNode, env);
  case AssignDictVarAst:   return eval_assign_dict_var_expr((AssignDictVar *)astNode, env);
  case ImportAst:          return eval_import_stmt((ImportStmt *)astNode, env);
  case AssignListExprAst:  return eval_assign_list_expr((AssignListExpr *)astNode, env);
  case AssignDictExprAst:  return eval_assign_dict_expr((AssignDictExpr *)astNode, env);
  case UnaryExprAst:       return eval_unary_expr((UnaryExpr *)astNode, env);
  case BreakAst:
    cf_signal = CF_BREAK;
    return (RuntimeVal *)MK_NIL();
  case ContinueAst:
    cf_signal = CF_CONTINUE;
    return (RuntimeVal *)MK_NIL();
  case ReturnAst: {
    ReturnStmt *ret = (ReturnStmt *)astNode;
    RuntimeVal *val = ret->value ? evaluate(&(ret->value->stmt), env) : (RuntimeVal *)MK_NIL();
    cf_set_return(val);
    return (RuntimeVal *)MK_NIL();
  }
  case ReturnSuccessAst: {
    ReturnStmt *ret = (ReturnStmt *)astNode;
    RuntimeVal *val = ret->value ? evaluate(&(ret->value->stmt), env) : (RuntimeVal *)MK_NIL();
    DictVal *res = MK_DICT(1);
    dict_set_val(res, "ok", val);
    cf_set_return((RuntimeVal *)res);
    release(val);
    return (RuntimeVal *)MK_NIL();
  }
  case ReturnErrorAst: {
    ReturnStmt *ret = (ReturnStmt *)astNode;
    RuntimeVal *val = ret->value ? evaluate(&(ret->value->stmt), env) : (RuntimeVal *)MK_NIL();
    DictVal *res = MK_DICT(1);
    dict_set_val(res, "err", val);
    cf_set_return((RuntimeVal *)res);
    release(val);
    return (RuntimeVal *)MK_NIL();
  }
  case UnwrapAst: {
    UnwrapExpr *u = (UnwrapExpr *)astNode;
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
  default: error("This AST Node has not yet been setup for interpretation.\n");
  }
  return NULL;
}

#include "../zox_alloc.h"

RuntimeVal *promote_val(RuntimeVal *val) {
  if (!val || val->ref_count == STATIC_REF) return val;
  if (!zox_arena_owns(val)) return val;

  switch (val->type) {
    case STRING_T:
      return (RuntimeVal *)MK_STRING(((StringVal *)val)->value);

    case LIST_T: {
      ListVal *old_list = (ListVal *)val;
      ListVal *new_list = MK_LIST(old_list->size);
      for (size_t i = 0; i < old_list->size; i++) {
        RuntimeVal *promoted_item = promote_val(old_list->items[i]);
        new_list->items[i] = promoted_item;
        retain(promoted_item);
      }
      new_list->size = old_list->size;
      return (RuntimeVal *)new_list;
    }

    case DICT_T: {
      DictVal *old_dict = (DictVal *)val;
      DictVal *new_dict = MK_DICT(old_dict->capacity);
      for (size_t i = 0; i < old_dict->capacity; i++) {
        if (old_dict->entries[i].key) {
          RuntimeVal *promoted_val = promote_val(old_dict->entries[i].value);
          dict_set_val(new_dict, old_dict->entries[i].key, promoted_val);
        }
      }
      return (RuntimeVal *)new_dict;
    }

    case FUNCTION_T: {
      FunctionVal *fv = (FunctionVal *)val;
      return (RuntimeVal *)MK_FUNCTION(fv->params, fv->param_count, fv->body, fv->body_count, fv->env, fv->builtin_func);
    }

    default:
      return val;
  }
}
