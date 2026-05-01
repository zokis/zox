/* ast_nodes.c - Criacao de todos os nos da AST e literais pre-alocados. */
#include "../ast.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../malloc_safe.h"

static NilLiteral     preallocated_nil_literal;
static BooleanLiteral preallocated_true_literal;
static BooleanLiteral preallocated_false_literal;
static NumericLiteral preallocated_numeric_literals[256];
static short int      numeric_literals_initialized = 0;

NilLiteral     *get_preallocated_nil(void)      { return &preallocated_nil_literal; }
BooleanLiteral *get_preallocated_true(void)     { return &preallocated_true_literal; }
BooleanLiteral *get_preallocated_false(void)    { return &preallocated_false_literal; }
NumericLiteral *get_preallocated_numerics(void) { return preallocated_numeric_literals; }

void initialize_preallocated_literals(void) {
  if (numeric_literals_initialized) return;
  preallocated_nil_literal.base.stmt.kind = NilAst;
  preallocated_true_literal.base.stmt.kind = BooleanLiteralAst;
  preallocated_true_literal.value = 1;
  preallocated_false_literal.base.stmt.kind = BooleanLiteralAst;
  preallocated_false_literal.value = 0;
  for (int i = 0; i < 256; i++) {
    preallocated_numeric_literals[i].base.stmt.kind = NumericLiteralAst;
    preallocated_numeric_literals[i].value = i * 1.0;
  }
  numeric_literals_initialized = 1;
}

Program *create_program(Stmt **body, size_t body_count) {
  initialize_preallocated_literals();
  Program *program = (Program *)malloc_safe(sizeof(Program), "Program");
  program->base.kind = ProgramAst;
  program->body = body;
  program->body_count = body_count;
  return program;
}

VarDeclaration *create_var_expr(const char *varname, Expr *value) {
  VarDeclaration *var_expr = (VarDeclaration *)malloc_safe(sizeof(VarDeclaration), "VarDeclaration");
  var_expr->base.stmt.kind = VarDeclarationAst;
  var_expr->varname = strdup(varname);
  var_expr->value = value;
  return var_expr;
}

AssignVar *assign_var_expr(const char *varname, Expr *value) {
  AssignVar *var_expr = (AssignVar *)malloc_safe(sizeof(AssignVar), "AssignVar");
  var_expr->base.stmt.kind = AssignVarAst;
  var_expr->varname = strdup(varname);
  var_expr->value = value;
  return var_expr;
}

AssignListVar *assign_list_expr(const char *varname, Expr *index, Expr *value) {
  AssignListVar *var_expr = (AssignListVar *)malloc_safe(sizeof(AssignListVar), "AssignListVar");
  var_expr->base.stmt.kind = AssignListVarAst;
  var_expr->varname = strdup(varname);
  var_expr->index = index;
  var_expr->value = value;
  return var_expr;
}

AssignDictVar *assign_dict_expr(const char *varname, Expr *key, Expr *value) {
  AssignDictVar *var_expr = (AssignDictVar *)malloc_safe(sizeof(AssignDictVar), "AssignDictVar");
  var_expr->base.stmt.kind = AssignDictVarAst;
  var_expr->varname = strdup(varname);
  var_expr->key = key;
  var_expr->value = value;
  return var_expr;
}

UnaryExpr *create_unary_expr(const char *operator, Expr *expr) {
  UnaryExpr *unary_expr = (UnaryExpr *)malloc_safe(sizeof(UnaryExpr), "UnaryExpr");
  unary_expr->base.stmt.kind = UnaryExprAst;
  unary_expr->operator = strdup(operator);
  unary_expr->expr = expr;
  return unary_expr;
}

BinaryExpr *create_binary_expr(Expr *left, Expr *right, const char *operator) {
  BinaryExpr *binary_expr = (BinaryExpr *)malloc_safe(sizeof(BinaryExpr), "BinaryExpr");
  binary_expr->base.stmt.kind = BinaryExprAst;
  binary_expr->left = left;
  binary_expr->right = right;
  binary_expr->operator = strdup(operator);
  return binary_expr;
}

Identifier *create_identifier(const char *symbol) {
  Identifier *identifier = (Identifier *)malloc_safe(sizeof(Identifier), "Identifier");
  identifier->base.stmt.kind = IdentifierAst;
  identifier->symbol = strdup(symbol);
  return identifier;
}

NumericLiteral *create_numeric_literal(double value) {
  if (value == floor(value) && value >= 0 && value <= 255) {
    return &preallocated_numeric_literals[(int)value];
  }
  NumericLiteral *numeric_literal = (NumericLiteral *)malloc_safe(sizeof(NumericLiteral), "NumericLiteral");
  numeric_literal->base.stmt.kind = NumericLiteralAst;
  numeric_literal->value = value;
  return numeric_literal;
}

StringLiteral *create_string_literal(const char *value) {
  StringLiteral *str_literal = (StringLiteral *)malloc_safe(sizeof(StringLiteral), "StringLiteral");
  str_literal->base.stmt.kind = StringLiteralAst;
  str_literal->value = strdup(value);
  return str_literal;
}

BooleanLiteral *create_boolean_literal(unsigned short int value) {
  return value ? &preallocated_true_literal : &preallocated_false_literal;
}

NilLiteral *create_nil_literal(void) { return &preallocated_nil_literal; }

WhileExpr *create_while(Expr *condition, Stmt **body, size_t body_count) {
  WhileExpr *while_expr = (WhileExpr *)malloc_safe(sizeof(WhileExpr), "WhileExpr");
  while_expr->base.stmt.kind = WhileAst;
  while_expr->condition = condition;
  while_expr->body = body;
  while_expr->body_count = body_count;
  return while_expr;
}

IfExpr *create_if(Expr *condition, Stmt **body, size_t body_count,
                  IfExpr *else_if, Stmt **else_body, size_t else_body_count) {
  IfExpr *if_expr = (IfExpr *)malloc_safe(sizeof(IfExpr), "IfExpr");
  if_expr->base.stmt.kind = IfAst;
  if_expr->condition = condition;
  if_expr->body = body;
  if_expr->body_count = body_count;
  if_expr->else_if = (struct IfExpr *)else_if;
  if_expr->else_body = else_body;
  if_expr->else_body_count = else_body_count;
  return if_expr;
}

ForExpr *create_for_expr(Expr *initialization, Expr *condition, Expr *increment,
                         Stmt **body, size_t body_count) {
  ForExpr *for_expr = (ForExpr *)malloc_safe(sizeof(ForExpr), "ForExpr");
  for_expr->base.stmt.kind = ForAst;
  for_expr->initialization = initialization;
  for_expr->condition = condition;
  for_expr->increment = increment;
  for_expr->body = body;
  for_expr->body_count = body_count;
  return for_expr;
}

FuncDef *create_func_def(char *name, char **params, size_t param_count,
                         Stmt **body, size_t body_count) {
  FuncDef *func_def = (FuncDef *)malloc_safe(sizeof(FuncDef), "FuncDef");
  func_def->base.stmt.kind = FuncDefAst;
  func_def->name = name;
  func_def->params = params;
  func_def->param_count = param_count;
  func_def->body = body;
  func_def->body_count = body_count;
  return func_def;
}

CallExpr *create_call_expr(Expr *callee, Expr **arguments, size_t arg_count) {
  CallExpr *call_expr = (CallExpr *)malloc_safe(sizeof(CallExpr), "CallExpr");
  call_expr->base.stmt.kind = CallExprAst;
  call_expr->callee = callee;
  call_expr->arguments = arguments;
  call_expr->arg_count = arg_count;
  return call_expr;
}

ListLiteral *create_list_literal(Expr **elements, size_t element_count) {
  ListLiteral *list = (ListLiteral *)malloc_safe(sizeof(ListLiteral), "ListLiteral");
  list->base.stmt.kind = ListLiteralAst;
  list->elements = elements;
  list->element_count = element_count;
  return list;
}

DictKey *create_dict_key(Expr *dict, Expr *key) {
  DictKey *dict_key = (DictKey *)malloc_safe(sizeof(DictKey), "DictKey");
  dict_key->base.stmt.kind = DictKeyAst;
  dict_key->dict = dict;
  dict_key->key = key;
  return dict_key;
}

ListIndex *create_list_index(Expr *list, Expr *start, Expr *end, short int is_slice) {
  ListIndex *list_index = (ListIndex *)malloc_safe(sizeof(ListIndex), "ListIndex");
  list_index->base.stmt.kind = ListIndexAst;
  list_index->list = list;
  list_index->start = start;
  list_index->end = end;
  list_index->is_slice = is_slice;
  return list_index;
}

DictLiteral *create_dict_literal(Expr **keys, Expr **values, size_t element_count) {
  DictLiteral *dict = (DictLiteral *)malloc_safe(sizeof(DictLiteral), "DictLiteral");
  dict->base.stmt.kind = DictLiteralAst;
  dict->keys = keys;
  dict->values = values;
  dict->element_count = element_count;
  return dict;
}

AssignListExpr *assign_list_expr_node(Expr *target, Expr *index, Expr *value) {
  AssignListExpr *n = malloc_safe(sizeof(AssignListExpr), "AssignListExpr");
  n->base.stmt.kind = AssignListExprAst;
  n->target = target;
  n->index  = index;
  n->value  = value;
  return n;
}

AssignDictExpr *assign_dict_expr_node(Expr *target, Expr *key, Expr *value) {
  AssignDictExpr *n = malloc_safe(sizeof(AssignDictExpr), "AssignDictExpr");
  n->base.stmt.kind = AssignDictExprAst;
  n->target = target;
  n->key    = key;
  n->value  = value;
  return n;
}

BreakStmt *create_break(void) {
  BreakStmt *s = malloc_safe(sizeof(BreakStmt), "BreakStmt");
  s->base.kind = BreakAst;
  return s;
}

ContinueStmt *create_continue(void) {
  ContinueStmt *s = malloc_safe(sizeof(ContinueStmt), "ContinueStmt");
  s->base.kind = ContinueAst;
  return s;
}

ReturnStmt *create_return(Expr *value) {
  ReturnStmt *s = malloc_safe(sizeof(ReturnStmt), "ReturnStmt");
  s->base.kind = ReturnAst;
  s->value = value;
  return s;
}
