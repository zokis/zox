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

Token expect(Parser *parser, ZoxTokenType type, const char *err) {
  Token token = eat(parser);
  if (token.type != type) parser_error(err, &token, type);
  return token;
}

static Stmt *parse_return_stmt(Parser *parser, ZoxTokenType type) {
  Expr *val = NULL;
  if (at(parser).type != SemiColonTk && at(parser).type != CloseBraceTk &&
      at(parser).type != EOFTk) {
    val = parse_expr(parser);
  }
  if (type == ReturnSuccessTk) return (Stmt *)create_return_success(val);
  if (type == ReturnErrorTk) return (Stmt *)create_return_error(val);
  return (Stmt *)create_return(val);
}

Stmt *parse_stmt(Parser *parser) {
  Stmt *stmt = NULL;

  switch (at(parser).type) {
    case ImportTk:
      stmt = parse_import_stmt(parser);
      break;
    case TypeTk:
      stmt = parse_type_declaration(parser);
      break;
    case LetTk:
      stmt = (Stmt *)parse_var_declaration(parser);
      break;
    case WhileTk:
      eat(parser);
      stmt = (Stmt *)parse_while_expr(parser);
      break;
    case ForTk:
      eat(parser);
      stmt = (Stmt *)parse_for_expr(parser);
      break;
    case IfTk:
      eat(parser);
      stmt = (Stmt *)parse_if_expr(parser);
      break;
    case BreakTk:
      eat(parser);
      stmt = (Stmt *)create_break();
      break;
    case ContinueTk:
      eat(parser);
      stmt = (Stmt *)create_continue();
      break;
    case ReturnTk:
    case ReturnSuccessTk:
    case ReturnErrorTk:
      stmt = parse_return_stmt(parser, eat(parser).type);
      break;
    default:
      stmt = (Stmt *)parse_expr(parser);
      break;
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
