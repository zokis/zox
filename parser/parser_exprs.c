/* parser_exprs.c - Expressoes: aritmeticas, logicas, bitwise, unarias, literais, identifiers. */
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
    if (token.value[0] != '<' && token.value[0] != '>' &&
        strcmp(token.value, "<=") != 0 && strcmp(token.value, ">=") != 0) {
      break;
    }
    char *operator = eat(parser).value;
    Expr *right = parse_additive_expr(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, operator);
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
    if (token.type == ArrowTk ||
        (token.value[0] != '+' && token.value[0] != '-' &&
         strcmp(token.value, "&+") != 0 && strcmp(token.value, "&-") != 0)) {
      break;
    }
    char *operator = eat(parser).value;
    Expr *right = parse_unary_expr(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, operator);
  }
  return left;
}

Expr *parse_bitwise_expr(Parser *parser) {
  Expr *left = parse_multiplicative_expr(parser);
  while (1) {
    Token token = at(parser);
    if (token.value[0] != '^' && token.value[0] != '&' &&
        token.value[0] != '|' && strcmp(token.value, "<<") != 0 &&
        strcmp(token.value, ">>") != 0 && strcmp(token.value, "&>>") != 0 &&
        strcmp(token.value, "&<<") != 0 && strcmp(token.value, "&|") != 0 &&
        strcmp(token.value, "&e") != 0 && strcmp(token.value, "&^") != 0) {
      break;
    }
    char *operator = eat(parser).value;
    Expr *right = parse_multiplicative_expr(parser);
    left = (Expr *)create_binary_expr(left, right, operator);
  }
  return left;
}

Expr *parse_multiplicative_expr(Parser *parser) {
  Expr *left = parse_unary_expr(parser);
  if (!left) return NULL;
  while (1) {
    Token token = at(parser);
    if (token.type == ArrowTk ||
        (token.value[0] != '/' && token.value[0] != '*' &&
         token.value[0] != '%' && strcmp(token.value, "&*") != 0 &&
         strcmp(token.value, "&/") != 0 && strcmp(token.value, "&%") != 0)) {
      break;
    }
    char *operator = eat(parser).value;
    Expr *right = (Expr *)parse_unary_expr(parser);
    if (!right) { free_expr(left); return NULL; }
    left = (Expr *)create_binary_expr(left, right, operator);
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
    elements = realloc_safe(elements, sizeof(Expr *) * (element_count + 1),
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
    keys = realloc_safe(keys, sizeof(Expr *) * (element_count + 1),
                        "parse_dict_literal keys");
    values = realloc_safe(values, sizeof(Expr *) * (element_count + 1),
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
  char *parsed_value = malloc_safe(
      strlen(raw_value) + 1, "Failed to allocate memory for parsed string");
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
        /* identifier simples (varname): usa assign_list_expr classico */
        if (identifier->stmt.kind == IdentifierAst) {
          free_expr(identifier);
          return (Expr *)assign_list_expr(varname, start,
                                          (Expr *)parse_expr(parser));
        }
        /* identifier composto (ex: b[3]): usa no generico */
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
        /* identifier simples (varname): usa assign_dict_expr classico */
        if (identifier->stmt.kind == IdentifierAst) {
          free_expr(identifier);
          return (Expr *)assign_dict_expr(varname, key,
                                          (Expr *)parse_expr(parser));
        }
        /* identifier composto (ex: b[3]): usa no generico */
        return (Expr *)assign_dict_expr_node(identifier, key,
                                             (Expr *)parse_expr(parser));
      }
      identifier = (Expr *)create_dict_key(identifier, key);
    } else {
      break;
    }
  }
  return identifier;
}

Expr *parse_primary_expr(Parser *parser) {
  TokenType tk = at(parser).type;

  switch (tk) {
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
    free_safe(parsed_value);
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
