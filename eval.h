#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "ast.h"
#include "env.h"
#include "values.h"

RuntimeVal *eval_program(Program *program, Environment *env);
NumberVal  *eval_numeric_binary_expr(NumberVal *lhs, NumberVal *rhs, const char *operator);
RuntimeVal *eval_binary_expr(BinaryExpr *binop, Environment *env);
RuntimeVal *evaluate(Stmt *astNode, Environment *env);
RuntimeVal *eval_list_literal(ListLiteral *list_lit, Environment *env);
void        dict_set_val(DictVal *dict, const char *key, RuntimeVal *value);
void        list_append_val(ListVal *list, RuntimeVal *item);
RuntimeVal *zox_call_function(FunctionVal *func, Environment *env, RuntimeVal **args, size_t arg_count);

#endif  /* EVALUATOR_H */
