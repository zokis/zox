#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

/* Internal parser header. */

#include "../parser.h"
#include "../ast.h"
#include "../global.h"
#include "../lexer.h"
#include "../malloc_safe.h"

#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern ExecutionContext global_context;

/* Base helpers from parser_core.c. */
short int not_eof(Parser *parser);
Token at(Parser *parser);
Token lookahead(Parser *parser, int distance);
Token eat(Parser *parser);

Token     expect(Parser *parser, TokenType type, const char *err);

/* Parse function prototypes. */
Expr *parse_expr(Parser *parser);
Expr *parse_relational_expr(Parser *parser);
Expr *parse_logical_or(Parser *parser);
Expr *parse_logical_and(Parser *parser);
Expr *parse_equality(Parser *parser);
Expr *parse_comparison(Parser *parser);
Expr *parse_unary_expr(Parser *parser);
Expr *parse_additive_expr(Parser *parser);
Expr *parse_bitwise_expr(Parser *parser);
Expr *parse_multiplicative_expr(Parser *parser);
Expr *parse_list_literal(Parser *parser);
Expr *parse_dict_literal(Parser *parser);
char *parse_string(const char *raw_value);
Expr *parse_identifier_expr(Parser *parser);
Stmt *parse_import_stmt(Parser *parser);
Expr *parse_primary_expr(Parser *parser);
Expr *parse_match_expr(Parser *parser);
Expr *parse_arena_block(Parser *parser);
Expr *parse_if_expr(Parser *parser);
Expr *parse_while_expr(Parser *parser);
Expr *parse_for_expr(Parser *parser);
Expr *parse_func_def(Parser *parser);
Expr *parse_call_expr(Parser *parser, Expr *callee);
Stmt *parse_stmt(Parser *parser);

#endif /* PARSER_INTERNAL_H */
