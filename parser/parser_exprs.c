/* Expression parsing. */
#include "parser_internal.h"

Expr *parse_expr(Parser *parser) { return parse_relational_expr(parser); }

Expr *parse_relational_expr(Parser *parser) { return parse_logical_or(parser); }

Expr *parse_logical_or(Parser *parser) {
  Expr *left = parse_logical_and(parser);
  if (!left) return NULL;
  while (1) {
    Token token = at(parser);
    if (strcmp(token.value, "||") != 0) break;
    char *operator = eat(parser).value;
    Expr *right = parse_logical_and(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, operator);
  }
  return left;
}

Expr *parse_logical_and(Parser *parser) {
  Expr *left = parse_equality(parser);
  if (!left) return NULL;
  while (1) {
    Token token = at(parser);
    if (strcmp(token.value, "&&") != 0) break;
    char *operator = eat(parser).value;
    Expr *right = parse_equality(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, operator);
  }
  return left;
}

Expr *parse_equality(Parser *parser) {
  Expr *left = parse_comparison(parser);
  if (!left) return NULL;
  while (1) {
    Token token = at(parser);
    if (strcmp(token.value, "==") != 0 && strcmp(token.value, "!=") != 0) break;
    char *operator = eat(parser).value;
    Expr *right = parse_comparison(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, operator);
  }
  return left;
}

Expr *parse_comparison(Parser *parser) {
  Expr *left = parse_additive_expr(parser);
  if (!left) return NULL;
  while (1) {
    Token token = at(parser);
    if (token.type != BinaryOperatorTk) break;
    const char *op = token.value;
    if (strcmp(op, "<") != 0 && strcmp(op, ">") != 0 &&
        strcmp(op, "<=") != 0 && strcmp(op, ">=") != 0) {
      break;
    }
    eat(parser);
    Expr *right = parse_additive_expr(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, op);
  }
  return left;
}

Expr *parse_unary_expr(Parser *parser) {
  Token token = at(parser);
  if (token.type == UnaryOperatorTk ||
      (token.type == BinaryOperatorTk &&
       (strcmp(token.value, "-") == 0 || strcmp(token.value, "+") == 0))) {
    char *operator = eat(parser).value;
    Expr *expr = parse_unary_expr(parser);
    return (Expr *)create_unary_expr(operator, expr);
  }
  return parse_primary_expr(parser);
}

Expr *parse_additive_expr(Parser *parser) {
  Expr *left = (Expr *)parse_bitwise_expr(parser);
  if (!left) return NULL;
  while (1) {
    Token token = at(parser);
    if (token.type == ArrowTk) break;
    if (token.type != BinaryOperatorTk) break;
    const char *op = token.value;
    if (strcmp(op, "+") != 0 && strcmp(op, "-") != 0 &&
        strcmp(op, "&+") != 0 && strcmp(op, "&-") != 0) {
      break;
    }
    eat(parser);
    Expr *right = parse_bitwise_expr(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, op);
  }
  return left;
}

Expr *parse_bitwise_expr(Parser *parser) {
  Expr *left = parse_multiplicative_expr(parser);
  while (1) {
    Token token = at(parser);
    if (token.type != BinaryOperatorTk) break;
    
    const char *op = token.value;
    /* Only these are bitwise */
    if (strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0 ||
        strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0 ||
        strcmp(op, "&>>") == 0 || strcmp(op, "&<<") == 0 ||
        strcmp(op, "&|") == 0 || strcmp(op, "&e") == 0 ||
        strcmp(op, "&^") == 0) {
      eat(parser);
      Expr *right = parse_multiplicative_expr(parser);
      if (!right) { free_expr(left); return NULL; }
      left = (Expr *)create_binary_expr(left, right, op);
    } else {
      break;
    }
  }
  return left;
}

Expr *parse_multiplicative_expr(Parser *parser) {
  Expr *left = parse_unary_expr(parser);
  if (!left) return NULL;
  while (1) {
    Token token = at(parser);
    if (token.type == ArrowTk) break;
    if (token.type != BinaryOperatorTk) break;
    const char *op = token.value;
    if (strcmp(op, "/") != 0 && strcmp(op, "*") != 0 &&
        strcmp(op, "%") != 0 && strcmp(op, "**") != 0 &&
        strcmp(op, "&*") != 0 && strcmp(op, "&/") != 0 && 
        strcmp(op, "&%") != 0) {
      break;
    }
    eat(parser);
    Expr *right = (Expr *)parse_unary_expr(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, op);
  }
  return left;
}

Expr *parse_list_literal(Parser *parser) {
  eat(parser);
  Expr **elements = NULL;
  size_t element_count = 0;
  while (at(parser).type != CloseBraceTk) {
    if (element_count > 0) {
      expect(parser, CommaTk, "Expected ',' between list elements.");
    }
    elements = zox_realloc_buf(ZOX_BUF_AST, elements, sizeof(Expr *) * (element_count + 1),
                               "parse_list_literal elements");
    elements[element_count++] = (Expr *)parse_expr(parser);
  }
  expect(parser, CloseBraceTk, "Expected '}' after list elements.");
  return (Expr *)create_list_literal(elements, element_count);
}

Expr *parse_dict_literal(Parser *parser) {
  expect(parser, OpenBracketTk,
         "Expected '[' at the beginning of dictionary literal.");
  Expr **keys = NULL;
  Expr **values = NULL;
  size_t element_count = 0;

  while (at(parser).type != CloseBracketTk) {
    if (element_count > 0) {
      expect(parser, SemiColonTk, "Expected ';' between dictionary elements.");
    }
    keys = zox_realloc_buf(ZOX_BUF_AST, keys, sizeof(Expr *) * (element_count + 1),
                           "parse_dict_literal keys");
    values = zox_realloc_buf(ZOX_BUF_AST, values, sizeof(Expr *) * (element_count + 1),
                             "parse_dict_literal values");
    Expr *key = (Expr *)parse_expr(parser);
    if (!key) break;
    keys[element_count] = key;
    expect(parser, ArrowTk, "Expected '->' between key and value.");
    Expr *value = (Expr *)parse_expr(parser);
    if (!value) break;
    values[element_count] = value;
    element_count++;
  }
  expect(parser, CloseBracketTk, "Expected ']' at the end of dictionary literal.");
  return (Expr *)create_dict_literal(keys, values, element_count);
}

char *parse_string(const char *raw_value) {
  char *parsed_value = zox_alloc_buf(
      ZOX_BUF_STRING, strlen(raw_value) + 1, "Failed to allocate memory for parsed string");
  int i = 0, j = 0;
  while (raw_value[i]) {
    if (raw_value[i] == '\\' && raw_value[i + 1]) {
      switch (raw_value[i + 1]) {
      case 'n':  parsed_value[j++] = '\n'; break;
      case 't':  parsed_value[j++] = '\t'; break;
      case 'r':  parsed_value[j++] = '\r'; break;
      case 'b':  parsed_value[j++] = '\b'; break;
      case 'f':  parsed_value[j++] = '\f'; break;
      case '"':  parsed_value[j++] = '"';  break;
      case '\'': parsed_value[j++] = '\''; break;
      case '\\': parsed_value[j++] = '\\'; break;
      default:   parsed_value[j++] = raw_value[i];
      }
      i += 2;
    } else {
      parsed_value[j++] = raw_value[i++];
    }
  }
  parsed_value[j] = '\0';
  return parsed_value;
}

Expr *parse_member_expr(Parser *parser, Expr *object) {
  expect(parser, DotTk, "Expected '.'");
  Token member = expect(parser, IdentifierTk, "Expected field name after '.'.");

  if (at(parser).type == EqualsTk) {
    eat(parser);
    Expr *value = parse_expr(parser);
    return (Expr *)create_assign_member_expr(
        object, zox_strdup_buf(ZOX_BUF_STRING, member.value), value);
  }

  Expr *identifier = (Expr *)create_member_expr(
      object, zox_strdup_buf(ZOX_BUF_STRING, member.value));

  while (1) {
    if (at(parser).type == OpenParenTk) {
      identifier = (Expr *)parse_call_expr(parser, identifier);
    } else if (at(parser).type == DotTk) {
      eat(parser);
      Token next_member = expect(parser, IdentifierTk, "Expected field name after '.'.");
      if (at(parser).type == EqualsTk) {
        eat(parser);
        Expr *value = parse_expr(parser);
        return (Expr *)create_assign_member_expr(
            identifier, zox_strdup_buf(ZOX_BUF_STRING, next_member.value), value);
      }
      identifier = (Expr *)create_member_expr(
          identifier, zox_strdup_buf(ZOX_BUF_STRING, next_member.value));
    } else {
      break;
    }
  }
  return identifier;
}

Expr *parse_identifier_expr(Parser *parser) {
  const char *varname = eat(parser).value;
  Expr *identifier = (Expr *)create_identifier(varname);

  while (1) {
    if (at(parser).type == EqualsTk) {
      eat(parser);
      free_expr(identifier);
      return (Expr *)assign_var_expr(varname, (Expr *)parse_expr(parser));
    } else if (at(parser).type == OpenParenTk) {
      identifier = (Expr *)parse_call_expr(parser, identifier);
    } else if (at(parser).type == OpenBracketTk) {
      eat(parser);
      Expr *start = NULL;
      Expr *end = NULL;
      short int is_slice = 0;
      if (at(parser).type == ElseTk) {
        eat(parser);
        start = (Expr *)create_numeric_literal(0);
        if (at(parser).type != CloseBracketTk) {
          end = (Expr *)parse_expr(parser);
        }
        is_slice = 1;
      } else {
        start = (Expr *)parse_expr(parser);
        if (at(parser).type == ElseTk) {
          eat(parser);
          if (at(parser).type != CloseBracketTk) {
            end = (Expr *)parse_expr(parser);
          }
          is_slice = 1;
        }
      }
      expect(parser, CloseBracketTk, "Expected ']' after list index.");
      if (at(parser).type == EqualsTk) {
        eat(parser);
        if (identifier->stmt.kind == IdentifierAst) {
          free_expr(identifier);
          return (Expr *)assign_list_expr(varname, start,
                                          (Expr *)parse_expr(parser));
        }
        return (Expr *)assign_list_expr_node(identifier, start,
                                             (Expr *)parse_expr(parser));
      }
      identifier = (Expr *)create_list_index(identifier, start, end, is_slice);
    } else if (at(parser).type == OpenBraceTk) {
      eat(parser);
      Expr *key = (Expr *)parse_expr(parser);
      expect(parser, CloseBraceTk, "Expected '}' after dict key.");
      if (at(parser).type == EqualsTk) {
        eat(parser);
        if (identifier->stmt.kind == IdentifierAst) {
          free_expr(identifier);
          return (Expr *)assign_dict_expr(varname, key,
                                          (Expr *)parse_expr(parser));
        }
        return (Expr *)assign_dict_expr_node(identifier, key,
                                             (Expr *)parse_expr(parser));
      }
      identifier = (Expr *)create_dict_key(identifier, key);
    } else if (at(parser).type == DotTk) {
      identifier = parse_member_expr(parser, identifier);
    } else if (at(parser).type == UnwrapTk) {
      eat(parser);
      identifier = (Expr *)create_unwrap_expr(identifier);
    } else {
      break;
    }
  }
  return identifier;
}

Expr *parse_primary_expr(Parser *parser) {
  ZoxTokenType tk = at(parser).type;

  switch (tk) {
  case MatchTk:
    return parse_match_expr(parser);
  case OpenArenaTk:
    return parse_arena_block(parser);
  case ImportTk:
    return (Expr *)parse_import_stmt(parser);
  case FunctionTk:
    eat(parser);
    return (Expr *)parse_func_def(parser);
  case IdentifierTk:
    return (Expr *)parse_identifier_expr(parser);
  case NumberTk:
    return (Expr *)create_numeric_literal(atof(eat(parser).value));
  case StringTk: {
    const char *raw_value = eat(parser).value;
    char *parsed_value = parse_string(raw_value);
    Expr *result = (Expr *)create_string_literal(parsed_value);
    zox_free_buf(ZOX_BUF_STRING, parsed_value);
    return result;
  }
  case BooleanLiteralTk:
    return (Expr *)create_boolean_literal(
        strcmp(eat(parser).value, "true") == 0 ? 1 : 0);
  case OpenTableTk:
  case OpenBracketTk:
    return parse_dict_literal(parser);
  case OpenBraceTk:
    return parse_list_literal(parser);
  case NilTk:
    eat(parser);
    return (Expr *)create_nil_literal();
  case LetTk:
    return (Expr *)parse_var_declaration(parser);
  case OpenParenTk: {
    eat(parser);
    Expr *value = (Expr *)parse_expr(parser);
    expect(parser, CloseParenTk,
           "Unexpected token found inside parenthesised expression. Expected "
           "closing parenthesis.");
    return value;
  }
  case IfTk:
    eat(parser);
    return (Expr *)parse_if_expr(parser);
  case WhileTk:
    eat(parser);
    return (Expr *)parse_while_expr(parser);
  case ForTk:
    eat(parser);
    return (Expr *)parse_for_expr(parser);
  case BreakTk:
    eat(parser);
    return (Expr *)create_break();
  case ContinueTk:
    eat(parser);
    return (Expr *)create_continue();
  case ReturnTk: {
    eat(parser);
    Expr *val = NULL;
    if (at(parser).type != SemiColonTk && at(parser).type != CloseBraceTk &&
        at(parser).type != EOFTk) {
      val = parse_expr(parser);
    }
    return (Expr *)create_return(val);
  }
  default: {
    Token token = at(parser);
    parser_error("Unexpected token found during parsing.", &token, tk);
    return NULL;
  }
  }
}

Expr *parse_arena_block(Parser *parser) {
  expect(parser, OpenArenaTk, "Expected '|{' to start arena block.");
  size_t body_count = 0;
  Stmt **body = NULL;

  while (at(parser).type != CloseArenaTk && at(parser).type != EOFTk) {
    body = zox_realloc_buf(ZOX_BUF_AST, body, sizeof(Stmt *) * (body_count + 1),
                           "parse_arena_block body");
    body[body_count++] = parse_stmt(parser);
  }

  expect(parser, CloseArenaTk, "Expected '}|' to end arena block.");
  return (Expr *)create_arena_block(body, body_count);
}

Expr *parse_match_expr(Parser *parser) {
  expect(parser, MatchTk, "Expected '?\\?' to start match expression.");
  expect(parser, OpenParenTk, "Expected ')' after '?\\?' keyword.");
  Expr *target = parse_expr(parser);
  expect(parser, CloseParenTk, "Expected ')' after match target.");
  expect(parser, OpenBraceTk, "Expected '{' after match target.");

  size_t case_count = 0;
  MatchCase **cases = NULL;

  while (at(parser).type != CloseBraceTk && at(parser).type != EOFTk) {
    Expr *condition = NULL;
    if (at(parser).type == IdentifierTk && strcmp(at(parser).value, "_") == 0) {
      eat(parser);
      condition = NULL; /* Wildcard */
    } else {
      condition = parse_expr(parser);
    }
    expect(parser, FatArrowTk, "Expected '=>' after match condition.");
    Expr *branch = parse_expr(parser);
    
    cases = zox_realloc_buf(ZOX_BUF_AST, cases, sizeof(MatchCase *) * (case_count + 1),
                            "parse_match_expr cases");
    cases[case_count++] = create_match_case(condition, branch);
    
    if (at(parser).type == CommaTk) eat(parser);
  }

  expect(parser, CloseBraceTk, "Expected '}' to end match expression.");
  return (Expr *)create_match_expr(target, cases, case_count);
}
