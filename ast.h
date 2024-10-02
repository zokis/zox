#include <stddef.h>

#ifndef AST_H
#define AST_H

struct Expr;
struct Stmt;
struct Program;
struct IfExpr;

typedef enum {
  ProgramAst,         // 0
  BooleanLiteralAst,  // 1
  NilAst,             // 2
  NumericLiteralAst,  // 3
  IdentifierAst,      // 4
  UnaryExprAst,       // 5
  BinaryExprAst,      // 6
  VarDeclarationAst,  // 7
  AssignVarAst,       // 8
  IfAst,              // 9
  WhileAst,           // 10
  ForAst,             // 11
  StringLiteralAst,   // 12
  FuncDefAst,         // 13
  CallExprAst,        // 14
  ListLiteralAst,     // 15
  DictLiteralAst,     // 16
  ListIndexAst,       // 17
  DictKeyAst,         // 18
  AssignListVarAst,   // 19
  AssignDictVarAst,   // 20
  TableLiteralAst,    // 21
  ImportAst           // 22
} NodeType;

typedef struct Stmt {
  NodeType kind;
} Stmt;

typedef struct {
  Stmt base;
  Stmt **body;
  size_t body_count;
} Program;

typedef struct {
  Stmt stmt;
} Expr;

typedef struct {
  Expr base;
  const char *operator;
  Expr *expr;
} UnaryExpr;

typedef struct {
  Expr base;
  Expr *left;
  Expr *right;
  const char *operator;
} BinaryExpr;

typedef struct {
  Expr base;
  char *symbol;
} Identifier;

typedef struct {
  Expr base;
  double value;
} NumericLiteral;

typedef struct {
  Expr base;
  char *value;
} StringLiteral;

typedef struct {
  Expr base;
  unsigned int value;
} BooleanLiteral;

typedef struct {
  Expr base;
} NilLiteral;

typedef struct {
  Expr base;
  Expr *value;
  char *varname;
} VarDeclaration;

typedef struct {
  Expr base;
  Expr *value;
  char *varname;
} AssignVar;

typedef struct {
  Expr base;
  Expr *value;
  Expr *index;
  char *varname;
} AssignListVar;

typedef struct {
  Expr base;
  Expr *value;
  Expr *key;
  char *varname;
} AssignDictVar;

typedef struct {
  Expr base;
  Expr *condition;
  Stmt **body;
  size_t body_count;
  Stmt **else_body;
  size_t else_body_count;
  struct IfExpr *else_if;
} IfExpr;

typedef struct {
  Expr base;
  Expr *condition;
  Stmt **body;
  size_t body_count;
} WhileExpr;

typedef struct {
  Expr base;
  Expr *initialization;
  Expr *condition;
  Expr *increment;
  Stmt **body;
  size_t body_count;
} ForExpr;

typedef struct {
  Expr base;
  char *name;
  char **params;
  size_t param_count;
  Stmt **body;
  size_t body_count;
} FuncDef;

typedef struct {
  Expr base;
  Expr *callee;
  Expr **arguments;
  size_t arg_count;
} CallExpr;

typedef struct {
  Expr base;
  Expr **elements;
  size_t element_count;
} ListLiteral;

typedef struct {
  Expr base;
  Expr **keys;
  Expr **values;
  size_t element_count;
} DictLiteral;

typedef struct {
  Expr base;
  Expr *dict;
  Expr *key;
} DictKey;

typedef struct {
  Expr base;
  char **columns;
  size_t column_count;
} TableLiteral;

typedef struct {
  char *name;
  char *alias;
} ImportItem;

typedef struct {
  Stmt base;
  char *module_name;
  ImportItem **imports;
  size_t import_count;
} ImportStmt;

typedef struct {
  Expr base;
  Expr *list;
  Expr *index;
  Expr *start;
  Expr *end;
  int is_slice;
} ListIndex;

Program *create_program(Stmt **body, size_t body_count);
BinaryExpr *create_binary_expr(Expr *left, Expr *right, const char *operator);
UnaryExpr *create_unary_expr(const char *operator, Expr *expr);
Identifier *create_identifier(const char *symbol);
NumericLiteral *create_numeric_literal(double value);
BooleanLiteral *create_boolean_literal(unsigned short int value);
VarDeclaration *create_var_expr(const char *varname, Expr *value);
AssignVar *assign_var_expr(const char *varname, Expr *value);
AssignListVar *assign_list_expr(const char *varname, Expr *index, Expr *value);
AssignDictVar *assign_dict_expr(const char *varname, Expr *key, Expr *value);
NilLiteral *create_nil_literal();
IfExpr *create_if(Expr *condition, Stmt **body, size_t body_count,
                  IfExpr *else_if, Stmt **else_body, size_t else_body_count);
WhileExpr *create_while(Expr *condition, Stmt **body, size_t body_count);
ForExpr *create_for_expr(Expr *initialization, Expr *condition, Expr *increment,
                         Stmt **body, size_t body_count);
StringLiteral *create_string_literal(const char *value);
FuncDef *create_func_def(char *name, char **params, size_t param_count,
                         Stmt **body, size_t body_count);
CallExpr *create_call_expr(Expr *callee, Expr **arguments, size_t arg_count);
ListLiteral *create_list_literal(Expr **elements, size_t element_count);
ListIndex *create_list_index(Expr *list, Expr *start, Expr *end,
                             short int is_slice);
DictLiteral *create_dict_literal(Expr **keys, Expr **values,
                                 size_t element_count);
DictKey *create_dict_key(Expr *dict, Expr *key);
TableLiteral *create_table_literal(char **columns, size_t column_count);

void free_expr(Expr *expr);
void free_stmt(Stmt *stmt);
void free_program(Program *program);

#endif  // AST_H
