#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "values.h"
#include "ast.h"
#include "env.h"

RuntimeVal *eval_program(Program *program, Environment *env);
NumberVal *eval_numeric_binary_expr(NumberVal *lhs, NumberVal *rhs, const char *operator);
RuntimeVal *eval_binary_expr(BinaryExpr *binop, Environment *env);
RuntimeVal *evaluate(Stmt *astNode, Environment *env);
RuntimeVal *eval_list_literal(ListLiteral *list_lit, Environment *env);
void dict_set_val(DictVal *dict, const char *key, RuntimeVal *value);

#endif // EVALUATOR_H
