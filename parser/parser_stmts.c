/* Statement parsing. */
#include "parser_internal.h"

Expr *parse_var_declaration(Parser *parser) {
  eat(parser);
  Token varname =
      expect(parser, IdentifierTk,
             "Expected identifier name following let const keywords.");
  if (at(parser).type == SemiColonTk) {
    eat(parser);
    return (Expr *)create_var_expr(varname.value, (Expr *)create_nil_literal());
  }
  expect(parser, EqualsTk, "Variable declaration statment must end with semicolon.");
  Expr *value = (Expr *)parse_expr(parser);
  if (at(parser).type == OpenParenTk) {
    value = parse_call_expr(parser, value);
  }
  expect(parser, SemiColonTk, "Missing ;");
  return (Expr *)create_var_expr(varname.value, value);
}

Stmt *parse_import_stmt(Parser *parser) {
  expect(parser, ImportTk, "Expected '~>' for import statement");

  char *module_name;
  if (at(parser).type == IdentifierImportTk ||
      at(parser).type == IdentifierTk) {
    module_name = eat(parser).value;
  } else if (at(parser).type == StringTk) {
    /* direct path: ~> "./lib/json.so" { ... } */
    module_name = eat(parser).value;
  } else {
    Token tk = at(parser);
    parser_error("Expected module name in import statement", &tk,
                 IdentifierImportTk);
    return NULL;
  }

  ImportStmt *import_stmt =
      zox_alloc_buf(ZOX_BUF_MISC, sizeof(ImportStmt), "ImportStmt allocation");
  import_stmt->base.kind = ImportAst;
  import_stmt->module_name = zox_strdup_buf(ZOX_BUF_STRING, module_name);
  import_stmt->imports = NULL;
  import_stmt->import_count = 0;

  if (at(parser).type == OpenBraceTk) {
    do {
      eat(parser);
      char *name =
          expect(parser, IdentifierTk, "Expected identifier in import list")
              .value;
      char *alias = NULL;
      if (at(parser).type == AsTk) {
        eat(parser);
        alias = expect(parser, IdentifierTk,
                       "Expected alias after 'as' in import statement")
                    .value;
      }
      ImportItem *item =
          zox_alloc_buf(ZOX_BUF_MISC, sizeof(ImportItem), "ImportItem allocation");
      item->name = zox_strdup_buf(ZOX_BUF_STRING, name);
      item->alias = alias ? zox_strdup_buf(ZOX_BUF_STRING, alias) : NULL;
      import_stmt->imports =
          zox_realloc_buf(ZOX_BUF_AST, import_stmt->imports,
                          sizeof(ImportItem *) * (import_stmt->import_count + 1),
                          "ImportStmt imports realloc");
      import_stmt->imports[import_stmt->import_count++] = item;
    } while (at(parser).type == CommaTk);

    expect(parser, CloseBraceTk, "Expected '}' after import list");
  } else {
    error("Expected '{' after module name in import statement");
  }

  expect(parser, SemiColonTk, "Expected ';' after import statement");
  return (Stmt *)import_stmt;
}

Expr *parse_if_expr(Parser *parser) {
  expect(parser, OpenParenTk, "Expected '(' after '?' keyword.");
  Expr *cond = parse_expr(parser);
  expect(parser, CloseParenTk, "Expected ')' after '?' condition.");
  expect(parser, OpenBraceTk, "Expected '{' to start '?' body.");
  Stmt **body = NULL;
  size_t body_count = 0;
  while (at(parser).type != CloseBraceTk) {
    body = zox_realloc_buf(ZOX_BUF_AST, body, sizeof(Stmt *) * (body_count + 1),
                           "parse_if_expr body");
    body[body_count++] = parse_stmt(parser);
  }
  expect(parser, CloseBraceTk, "Expected '}' to close '?' body.");
  Stmt **else_body = NULL;
  size_t else_body_count = 0;
  IfExpr *else_if = NULL;
  while (at(parser).type == ElseTk) {
    eat(parser);
    if (at(parser).type == IfTk) {
      eat(parser);
      else_if = (IfExpr *)parse_if_expr(parser);
    } else {
      expect(parser, OpenBraceTk, "Expected '{' to start ':' body.");
      while (at(parser).type != CloseBraceTk) {
        else_body =
            zox_realloc_buf(ZOX_BUF_AST, else_body,
                            sizeof(Stmt *) * (else_body_count + 1),
                            "parse_if_expr else_body");
        else_body[else_body_count++] = parse_stmt(parser);
      }
      expect(parser, CloseBraceTk, "Expected '}' to close ':' body.");
      break;
    }
  }
  return (Expr *)create_if(cond, body, body_count, else_if, else_body,
                           else_body_count);
}

Expr *parse_while_expr(Parser *parser) {
  expect(parser, OpenParenTk, "Expected '(' after '#' keyword.");
  Expr *cond = parse_expr(parser);
  expect(parser, CloseParenTk, "Expected ')' after '#' condition.");
  expect(parser, OpenBraceTk, "Expected '{' to start '#' body.");
  Stmt **body = NULL;
  size_t body_count = 0;
  while (at(parser).type != CloseBraceTk) {
    body = zox_realloc_buf(ZOX_BUF_AST, body, sizeof(Stmt *) * (body_count + 1),
                           "parse_while_expr body");
    body[body_count++] = parse_stmt(parser);
  }
  expect(parser, CloseBraceTk, "Expected '}' to close '#' body.");
  return (Expr *)create_while(cond, body, body_count);
}

Expr *parse_for_expr(Parser *parser) {
  expect(parser, OpenParenTk, "Expected '(' after '@' keyword.");
  Expr *initialization = (Expr *)parse_stmt(parser);
  Expr *condition = (Expr *)parse_expr(parser);
  expect(parser, SemiColonTk, "Expected ';' after for condition.");
  Expr *increment = (Expr *)parse_expr(parser);
  expect(parser, CloseParenTk, "Expected ')' after for increment.");
  expect(parser, OpenBraceTk, "Expected '{' to start '@' body.");
  Stmt **body = NULL;
  size_t body_count = 0;
  while (at(parser).type != CloseBraceTk) {
    body = zox_realloc_buf(ZOX_BUF_AST, body, sizeof(Stmt *) * (body_count + 1),
                           "parse_for_expr body");
    body[body_count++] = parse_stmt(parser);
  }
  expect(parser, CloseBraceTk, "Expected '}' to close '@' body.");
  return (Expr *)create_for_expr(initialization, condition, increment, body,
                                 body_count);
}

Expr *parse_func_def(Parser *parser) {
  Token name_token =
      expect(parser, IdentifierTk, "Expected function name after '$'.");
  char *name = zox_strdup_buf(ZOX_BUF_STRING, name_token.value);
  expect(parser, OpenParenTk, "Expected '(' after function name.");
  char **params = NULL;
  size_t param_count = 0;
  while (at(parser).type != CloseParenTk) {
    if (param_count > 0) {
      expect(parser, CommaTk, "Expected ',' between function parameters.");
    }
    Token param = expect(parser, IdentifierTk, "Expected parameter name.");
    params = zox_realloc_buf(ZOX_BUF_AST, params, sizeof(char *) * (param_count + 1),
                             "parse_func_def params");
    params[param_count++] = zox_strdup_buf(ZOX_BUF_STRING, param.value);
  }
  expect(parser, CloseParenTk, "Expected ')' after function parameters.");
  expect(parser, OpenBraceTk, "Expected '{' to start function body.");
  Stmt **body = NULL;
  size_t body_count = 0;
  while (at(parser).type != CloseBraceTk) {
    body = zox_realloc_buf(ZOX_BUF_AST, body, sizeof(Stmt *) * (body_count + 1),
                           "parse_func_def body");
    body[body_count++] = parse_stmt(parser);
  }
  expect(parser, CloseBraceTk, "Expected '}' to end function body.");
  return (Expr *)create_func_def(name, params, param_count, body, body_count);
}

Expr *parse_call_expr(Parser *parser, Expr *callee) {
  expect(parser, OpenParenTk, "Expected '(' after function name.");
  Expr **args = NULL;
  size_t arg_count = 0;
  while (at(parser).type != CloseParenTk) {
    if (arg_count > 0) {
      expect(parser, CommaTk, "Expected ',' between arguments.");
    }
    args = zox_realloc_buf(ZOX_BUF_AST, args, sizeof(Expr *) * (arg_count + 1),
                           "parse_call_expr args");
    args[arg_count++] = parse_expr(parser);
  }
  expect(parser, CloseParenTk, "Expected ')' after arguments.");
  return (Expr *)create_call_expr(callee, args, arg_count);
}

Stmt *parse_type_declaration(Parser *parser) {
  expect(parser, TypeTk, "Expected 'type' keyword.");
  Token name = expect(parser, IdentifierTk, "Expected type name after 'type' keyword.");
  expect(parser, OpenBraceTk, "Expected '{' after type name.");

  char **fields = NULL;
  size_t count = 0;

  while (at(parser).type != CloseBraceTk && at(parser).type != EOFTk) {
    if (count > 0) {
      expect(parser, CommaTk, "Expected ',' between fields.");
    }
    Token field = expect(parser, IdentifierTk, "Expected field name.");
    fields = zox_realloc_buf(ZOX_BUF_AST, fields, sizeof(char *) * (count + 1),
                             "parse_type_declaration fields");
    fields[count++] = zox_strdup_buf(ZOX_BUF_STRING, field.value);
  }

  expect(parser, CloseBraceTk, "Expected '}' after fields.");
  return (Stmt *)create_type_declaration(zox_strdup_buf(ZOX_BUF_STRING, name.value), fields, count);
}
