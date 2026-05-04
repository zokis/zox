#ifndef EVAL_INTERNAL_H
#define EVAL_INTERNAL_H

/* Internal eval header. */

#include "../eval.h"
#include "../values.h"
#include "../env.h"
#include "../global.h"
#include "../hash.h"
#include "../native_modules.h"
#include "../parser.h"
#include "../zox_alloc.h"

#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

/* ControlFlow defined in eval_core.c. */
typedef enum { CF_NONE = 0, CF_BREAK, CF_CONTINUE, CF_RETURN } ControlFlowKind;

extern ControlFlowKind cf_signal;
extern RuntimeVal     *cf_return_val;

void        cf_set_return(RuntimeVal *val);
RuntimeVal *cf_take_return_val(void);

/* eval_ops.c */
NumberVal  *eval_numeric_binary_expr(NumberVal *lhs, NumberVal *rhs, const char *operator);
ListVal    *eval_list_binary_expr(ListVal *lhs, ListVal *rhs, const char *operator);
RuntimeVal *eval_list_any_binary_expr(const char *operator, ListVal *lhs, RuntimeVal *rhs);
DictVal    *eval_dict_binary_expr(DictVal *lhs, DictVal *rhs, const char *operator);
RuntimeVal *eval_string_repeat(StringVal *str, NumberVal *num);
RuntimeVal *eval_binary_expr_evaluated(RuntimeVal *lhs, RuntimeVal *rhs, const char *operator);
RuntimeVal *eval_unary_expr(UnaryExpr *unary_expr, Environment *env);
char       *runtime_value_to_string(RuntimeVal *val);
Expr       *runtime_value_to_expr(RuntimeVal *val);

/* eval_control.c */
short int   is_while_finished(WhileExpr *while_expr, Environment *env);
RuntimeVal *eval_while_expr(WhileExpr *while_expr, Environment *env);
RuntimeVal *eval_match_expr(MatchExpr *match_expr, Environment *env);
RuntimeVal *eval_member_expr(MemberExpr *member_expr, Environment *env);
RuntimeVal *eval_arena_block(ArenaBlockExpr *arena_expr, Environment *env);
RuntimeVal *eval_if_expr(IfExpr *if_expr, Environment *env);
RuntimeVal *eval_for_expr(ForExpr *for_expr, Environment *env);

/* eval_collections.c */
void        list_append_val(ListVal *list, RuntimeVal *item);
RuntimeVal *eval_list_literal(ListLiteral *list_lit, Environment *env);
void        resize_dict(DictVal *dict);
void        dict_set_val(DictVal *dict, const char *key, RuntimeVal *value);
RuntimeVal *eval_dict_literal(DictLiteral *dict_lit, Environment *env);
RuntimeVal *get_list_slice(ListVal *list, int start, int end);
RuntimeVal *get_string_slice(StringVal *str, int start, int end);
RuntimeVal *eval_list_index(ListIndex *list_index, Environment *env);
RuntimeVal *eval_dict_key(DictKey *dict_key, Environment *env);

/* eval_funcs.c */
RuntimeVal *eval_var_expr(VarDeclaration *var, Environment *env);
RuntimeVal *eval_string_literal(StringLiteral *str_literal);
RuntimeVal *eval_assign_var_expr(AssignVar *var, Environment *env);
RuntimeVal *eval_assign_list_var_expr(AssignListVar *var, Environment *env);
RuntimeVal *eval_assign_dict_var_expr(AssignDictVar *var, Environment *env);
RuntimeVal *eval_assign_list_expr(AssignListExpr *node, Environment *env);
RuntimeVal *eval_assign_dict_expr(AssignDictExpr *node, Environment *env);
RuntimeVal *eval_assign_member_expr(AssignMemberExpr *node, Environment *env);
RuntimeVal *eval_identifier_expr(Identifier *ident, Environment *env);
RuntimeVal *eval_func_def(FuncDef *func_def, Environment *env);
RuntimeVal *eval_type_declaration(TypeDeclaration *type_decl, Environment *env);
RuntimeVal *eval_call_expr(CallExpr *call_expr, Environment *env);

/* eval_import.c */
char       *find_module_path(const char *module_name);
RuntimeVal *eval_import_stmt(ImportStmt *import_stmt, Environment *env);

#endif /* EVAL_INTERNAL_H */
