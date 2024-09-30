#include "ast.h"

#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


#include "malloc_safe.h"

static NilLiteral preallocated_nil_literal;
static BooleanLiteral preallocated_true_literal;
static BooleanLiteral preallocated_false_literal;
static NumericLiteral preallocated_numeric_literals[256];
static short int numeric_literals_initialized = 0;

void initialize_preallocated_literals() {
  if (numeric_literals_initialized) return;
  preallocated_nil_literal.base.stmt.kind = NilAst;
  preallocated_true_literal.base.stmt.kind = BooleanLiteralAst;
  preallocated_true_literal.value = 1;
  preallocated_false_literal.base.stmt.kind = BooleanLiteralAst;
  preallocated_false_literal.value = 0;
  for (short int i = -1; i < 255; i++) {
    preallocated_numeric_literals[i].base.stmt.kind = NumericLiteralAst;
    preallocated_numeric_literals[i].value = (1 + i) * 1.0;
  }
  numeric_literals_initialized = 1;
}

Program *create_program(Stmt **body, size_t body_count) {
  initialize_preallocated_literals();
  Program *program = (Program *)malloc_safe(sizeof(Program), "Program");
  program->base.kind = ProgramAst;
  program->base.kind = ProgramAst;
  program->body = body;
  program->body_count = body_count;
  return program;
}

VarDeclaration *create_var_expr(const char *varname, Expr *value) {
  VarDeclaration *var_expr =
      (VarDeclaration *)malloc_safe(sizeof(VarDeclaration), "VarDeclaration");
  var_expr->base.stmt.kind = VarDeclarationAst;
  var_expr->varname = strdup(varname);
  var_expr->value = value;
  return var_expr;
}

AssignVar *assign_var_expr(const char *varname, Expr *value) {
  AssignVar *var_expr =
      (AssignVar *)malloc_safe(sizeof(AssignVar), "AssignVar");
  var_expr->base.stmt.kind = AssignVarAst;
  var_expr->varname = strdup(varname);
  var_expr->value = value;
  return var_expr;
}

AssignListVar *assign_list_expr(const char *varname, Expr *index, Expr *value) {
  AssignListVar *var_expr =
      (AssignListVar *)malloc_safe(sizeof(AssignListVar), "AssignListVar");
  var_expr->base.stmt.kind = AssignListVarAst;
  var_expr->varname = strdup(varname);
  var_expr->index = index;
  var_expr->value = value;
  return var_expr;
}

AssignDictVar *assign_dict_expr(const char *varname, Expr *key, Expr *value) {
  AssignDictVar *var_expr =
      (AssignDictVar *)malloc_safe(sizeof(AssignDictVar), "AssignDictVar");
  var_expr->base.stmt.kind = AssignDictVarAst;
  var_expr->varname = strdup(varname);
  var_expr->key = key;
  var_expr->value = value;
  return var_expr;
}

BinaryExpr *create_binary_expr(Expr *left, Expr *right, const char *operator) {
  BinaryExpr *binary_expr =
      (BinaryExpr *)malloc_safe(sizeof(BinaryExpr), "BinaryExpr");
  binary_expr->base.stmt.kind = BinaryExprAst;
  binary_expr->left = left;
  binary_expr->right = right;
  binary_expr->operator= strdup(operator);
  return binary_expr;
}

Identifier *create_identifier(const char *symbol) {
  Identifier *identifier =
      (Identifier *)malloc_safe(sizeof(Identifier), "Identifier");
  identifier->base.stmt.kind = IdentifierAst;
  identifier->symbol = strdup(symbol);
  return identifier;
}

NumericLiteral *create_numeric_literal(double value) {
  if (value == floor(value) && value >= 0 && value <= 255) {
    return &preallocated_numeric_literals[(int)value - 1];
  }
  NumericLiteral *numeric_literal =
      (NumericLiteral *)malloc_safe(sizeof(NumericLiteral), "NumericLiteral");
  numeric_literal->base.stmt.kind = NumericLiteralAst;
  numeric_literal->value = value;
  return numeric_literal;
}

StringLiteral *create_string_literal(const char *value) {
  StringLiteral *str_literal =
      (StringLiteral *)malloc_safe(sizeof(StringLiteral), "StringLiteral");
  str_literal->base.stmt.kind = StringLiteralAst;
  str_literal->value = strdup(value);
  return str_literal;
}

BooleanLiteral *create_boolean_literal(unsigned short int value) {
  if (value == 1) {
    return &preallocated_true_literal;
  } else {
    return &preallocated_false_literal;
  }
}

NilLiteral *create_nil_literal() { return &preallocated_nil_literal; }

WhileExpr *create_while(Expr *condition, Stmt **body, size_t body_count) {
  WhileExpr *while_expr =
      (WhileExpr *)malloc_safe(sizeof(WhileExpr), "WhileExpr");
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
  ListLiteral *list =
      (ListLiteral *)malloc_safe(sizeof(ListLiteral), "ListLiteral");
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

ListIndex *create_list_index(Expr *list, Expr *start, Expr *end,
                             short int is_slice) {
  ListIndex *list_index =
      (ListIndex *)malloc_safe(sizeof(ListIndex), "ListIndex");
  list_index->base.stmt.kind = ListIndexAst;
  list_index->list = list;
  list_index->start = start;
  list_index->end = end;
  list_index->is_slice = is_slice;
  return list_index;
}

DictLiteral *create_dict_literal(Expr **keys, Expr **values,
                                 size_t element_count) {
  DictLiteral *dict =
      (DictLiteral *)malloc_safe(sizeof(DictLiteral), "DictLiteral");
  dict->base.stmt.kind = DictLiteralAst;
  dict->keys = keys;
  dict->values = values;
  dict->element_count = element_count;
  return dict;
}

TableLiteral *create_table_literal(char **columns, size_t column_count) {
  TableLiteral *table =
      (TableLiteral *)malloc_safe(sizeof(TableLiteral), "TableLiteral");
  table->base.stmt.kind = TableLiteralAst;
  table->columns = columns;
  table->column_count = column_count;
  return table;
}

void free_expr(Expr *expr) {
  if (!expr) return;

  switch (expr->stmt.kind) {
    case StringLiteralAst: {
      StringLiteral *str_literal = (StringLiteral *)expr;
      free_safe(str_literal->value);
      free_safe(str_literal);
      break;
    }
    case BooleanLiteralAst: {
      if (expr == (Expr *)&preallocated_true_literal ||
          expr == (Expr *)&preallocated_false_literal) {
        return;
      }
      free_safe(expr);
      break;
    }
    case NilAst: {
      if (expr == (Expr *)&preallocated_nil_literal) {
        return;
      }
      free_safe(expr);
      break;
    }
    case IdentifierAst: {
      Identifier *identifier = (Identifier *)expr;
      free_safe(identifier->symbol);
      free_safe(identifier);
      break;
    }
    case BinaryExprAst: {
      BinaryExpr *binary_expr = (BinaryExpr *)expr;
      free_expr(binary_expr->left);
      free_expr(binary_expr->right);
      free_safe(binary_expr->operator);
      free_safe(binary_expr);
      break;
    }
    case ListLiteralAst: {
      ListLiteral *list_literal = (ListLiteral *)expr;
      for (size_t i = 0; i < list_literal->element_count; i++) {
        free_expr(list_literal->elements[i]);
      }
      free_safe(list_literal->elements);
      free_safe(list_literal);
      break;
    }
    case DictLiteralAst: {
      DictLiteral *dict_literal = (DictLiteral *)expr;
      for (size_t i = 0; i < dict_literal->element_count; i++) {
        free_expr(dict_literal->keys[i]);
        free_expr(dict_literal->values[i]);
      }
      free_safe(dict_literal->keys);
      free_safe(dict_literal->values);
      free_safe(dict_literal);
      break;
    }
    case CallExprAst: {
      CallExpr *call_expr = (CallExpr *)expr;
      free_expr(call_expr->callee);
      for (size_t i = 0; i < call_expr->arg_count; i++) {
        free_expr(call_expr->arguments[i]);
      }
      free_safe(call_expr->arguments);
      free_safe(call_expr);
      break;
    }
    default: {
      break;
    }
  }
}

void free_stmt(Stmt *stmt) {
  if (!stmt) return;

  switch (stmt->kind) {
    case ProgramAst: {
      Program *program = (Program *)stmt;
      for (size_t i = 0; i < program->body_count; i++) {
        free_stmt(program->body[i]);
      }
      free_safe(program->body);
      free_safe(program);
      break;
    }
    case VarDeclarationAst: {
      VarDeclaration *var_decl = (VarDeclaration *)stmt;
      free_safe(var_decl->varname);
      free_expr(var_decl->value);
      break;
    }
    case AssignVarAst: {
      AssignVar *assign_var = (AssignVar *)stmt;
      free_safe(assign_var->varname);
      free_expr(assign_var->value);
      break;
    }
    case BinaryExprAst: {
      BinaryExpr *binary_expr = (BinaryExpr *)stmt;
      free_expr(binary_expr->left);
      free_expr(binary_expr->right);
      free_safe(binary_expr->operator);
      break;
    }
    case ListLiteralAst: {
      ListLiteral *list_literal = (ListLiteral *)stmt;
      for (size_t i = 0; i < list_literal->element_count; i++) {
        free_expr(list_literal->elements[i]);
      }
      free_safe(list_literal->elements);
      break;
    }
    case DictLiteralAst: {
      DictLiteral *dict_literal = (DictLiteral *)stmt;
      for (size_t i = 0; i < dict_literal->element_count; i++) {
        free_expr(dict_literal->keys[i]);
        free_expr(dict_literal->values[i]);
      }
      free_safe(dict_literal->keys);
      free_safe(dict_literal->values);
      break;
    }
    case NilAst:
      break;
    case CallExprAst: {
      CallExpr *call_expr = (CallExpr *)stmt;
      free_expr(call_expr->callee);
      for (size_t i = 0; i < call_expr->arg_count; i++) {
        free_expr(call_expr->arguments[i]);
      }
      free_safe(call_expr->arguments);
      break;
    }
    default: {
      break;
    }
  }
  free_safe(stmt);
}

void free_program(Program *program) {
  for (size_t i = 0; i < program->body_count; i++) {
    free_stmt(program->body[i]);
  }
  free_safe(program->body);
  free_safe(program);
}