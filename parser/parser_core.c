/* Parser core: setup, base helpers, statements, AST production. */
#include "parser_internal.h"

Parser *create_parser(Token *tokens, long long int token_count) {
  Parser *parser = (Parser *)malloc_safe(sizeof(Parser), "Failed to allocate Parser");
  parser->tokens = tokens;
  parser->token_count = token_count;
  parser->current = 0;
  return parser;
}

short int not_eof(Parser *parser) {
  return parser->current < parser->token_count &&
         parser->tokens[parser->current].type != EOFTk;
}

Token at(Parser *parser) { return parser->tokens[parser->current]; }

Token lookahead(Parser *parser, int distance) {
  if (parser->current + distance >= parser->token_count) {
    return parser->tokens[parser->token_count - 1];
  }
  return parser->tokens[parser->current + distance];
}

Token eat(Parser *parser) {
  Token t = parser->tokens[parser->current++];
  /* keep runtime errors anchored to latest token */
  error_cursor.line   = t.line;
  error_cursor.column = t.column;
  return t;
}

Token expect(Parser *parser, TokenType type, const char *err) {
  Token token = eat(parser);
  if (token.type != type) parser_error(err, &token, type);
  return token;
}

Stmt *parse_stmt(Parser *parser) {
  Stmt *stmt = NULL;

  if (at(parser).type == ImportTk) {
    stmt = parse_import_stmt(parser);
  } else if (at(parser).type == LetTk) {
    stmt = (Stmt *)parse_var_declaration(parser);
  } else if (at(parser).type == WhileTk) {
    eat(parser);
    stmt = (Stmt *)parse_while_expr(parser);
  } else if (at(parser).type == ForTk) {
    eat(parser);
    stmt = (Stmt *)parse_for_expr(parser);
  } else if (at(parser).type == IfTk) {
    eat(parser);
    stmt = (Stmt *)parse_if_expr(parser);
  } else if (at(parser).type == BreakTk) {
    eat(parser);
    stmt = (Stmt *)create_break();
  } else if (at(parser).type == ContinueTk) {
    eat(parser);
    stmt = (Stmt *)create_continue();
  } else if (at(parser).type == ReturnTk || at(parser).type == ReturnSuccessTk ||
             at(parser).type == ReturnErrorTk) {
    TokenType type = eat(parser).type;
    Expr *val = NULL;
    if (at(parser).type != SemiColonTk && at(parser).type != CloseBraceTk &&
        at(parser).type != EOFTk) {
      val = parse_expr(parser);
    }
    if (type == ReturnSuccessTk) stmt = (Stmt *)create_return_success(val);
    else if (type == ReturnErrorTk)   stmt = (Stmt *)create_return_error(val);
    else stmt = (Stmt *)create_return(val);
  } else {
    stmt = (Stmt *)parse_expr(parser);
  }

  if (stmt != NULL && at(parser).type == SemiColonTk) {
    eat(parser);
  }

  return stmt;
}

Program *produce_ast(Parser *parser, const char *source_code) {
  Program *program = create_program(NULL, 0);
  while (not_eof(parser)) {
    program->body = (Stmt **)realloc_safe(
        program->body, sizeof(Stmt *) * (program->body_count + 1),
                                 "produce_ast");
    Stmt *stmt = parse_stmt(parser);
    if (stmt != NULL) program->body[program->body_count++] = stmt;
  }
  return program;
}
