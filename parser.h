#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
  Token *tokens;
  long long int token_count;
  long long int current;
} Parser;

Parser *create_parser(Token *tokens, long long int token_count);
short int not_eof(Parser *parser);
Token at(Parser *parser);
Token eat(Parser *parser);
Token expect(Parser *parser, TokenType type, const char *err);
Program *produce_ast(Parser *parser, const char *source_code);
Stmt *parse_stmt(Parser *parser);
Expr *parse_expr(Parser *parser);
Expr *parse_additive_expr(Parser *parser);
Expr *parse_multiplicative_expr(Parser *parser);
Expr *parse_var_declaration(Parser *parser);
Expr *parse_primary_expr(Parser *parser);
Expr *parse_relational_expr(Parser *parser);
Expr *parse_logical_or(Parser *parser);
Expr *parse_logical_and(Parser *parser);
Expr *parse_equality(Parser *parser);
Expr *parse_comparison(Parser *parser);
Expr *parse_if_expr(Parser *parser);
Expr *parse_while_expr(Parser *parser);
Expr *parse_for_expr(Parser *parser);
Expr *parse_func_def(Parser *parser);
Expr *parse_call_expr(Parser *parser, Expr *callee);
Expr *parse_list_literal(Parser *parser);
Expr *parse_bitwise_expr(Parser *parser);
Expr *parse_dict_literal(Parser *parser);
Expr *parse_identifier_expr(Parser *parser);
Expr *parse_table_literal(Parser *parser);
Expr *parse_index_expr(Parser *parser);
Expr *parse_assign_dict_expr(Parser *parser);
Expr *parse_assign_var_expr(Parser *parser);
Expr *parse_return_expr(Parser *parser);
Expr *parse_break_expr(Parser *parser);
Expr *parse_continue_expr(Parser *parser);
Stmt *parse_import_stmt(Parser *parser);

#endif  // PARSER_H
