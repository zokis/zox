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
  case ForAst:             return eval_for_expr((ForExpr *)astNode, env);
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
  default: error("This AST Node has not yet been setup for interpretation.\n");
  }
  return NULL;
}
