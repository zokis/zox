#include "lexer.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "malloc_safe.h"

Token create_token(const char *value, TokenType type, int line,
                   short int column) {
  Token t;
  t.value = strdup(value);
  t.type = type;
  t.line = line;
  t.column = column;
  return t;
}

int isalpha_custom(char c) {
  return isalpha(c) || (unsigned char)c >= 128;
}

int isskippable(char c) { return c == ' ' || c == '\n' || c == '\t' || c == '\r'; }

int isint(char c) { return isdigit(c); }

int isquote(char c) { return c == '"' || c == '\''; }

void ensure_capacity(Token **tokens, size_t *capacity, size_t tokenCount,
                     const char *errMsg) {
  if (tokenCount >= *capacity) {
    *capacity *= 2;
    *tokens = (Token *)realloc_safe(*tokens, *capacity * sizeof(Token), errMsg);
  }
}

int utf8_char_len(char c) {
  if ((c & 0x80) == 0)
    return 1;
  else if ((c & 0xE0) == 0xC0)
    return 2;
  else if ((c & 0xF0) == 0xE0)
    return 3;
  else if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

Token handle_ampersand_token(const char **src, int *line,
                             unsigned short int *column) {
  if (*(*src + 1) == '&') {
    *src += 2;
    *column += 2;
    return create_token("&&", BinaryOperatorTk, *line, *column - 2);
  } else if (*(*src + 1) == '+' || *(*src + 1) == '-' || *(*src + 1) == '*' ||
             *(*src + 1) == '/' || *(*src + 1) == '%' ||
             (*(*src + 1) == '*' && *(*src + 2) == '*') ||
             (*(*src + 1) == '<' && *(*src + 2) == '<') ||
             (*(*src + 1) == '>' && *(*src + 2) == '>') || *(*src + 1) == 'e' ||
             *(*src + 1) == '|' || *(*src + 1) == '~') {
    char op[4] = {'&', 0, 0, 0};
    int i = 1;
    (*src)++;
    (*column)++;
    if (**src == '*' && *(*src + 1) == '*') {
      op[i++] = *(*src)++;
      op[i++] = *(*src)++;
      *column += 2;
    } else if ((**src == '<' && *(*src + 1) == '<') ||
               (**src == '>' && *(*src + 1) == '>')) {
      op[i++] = *(*src)++;
      op[i++] = *(*src)++;
      *column += 2;
    } else {
      op[i++] = *(*src)++;
      (*column)++;
    }
    return create_token(op, BinaryOperatorTk, *line, *column - i);
  } else {
    (*src)++;
    (*column)++;
    return create_token("&", BinaryOperatorTk, *line, *column - 1);
  }
}

void add_token(Token **tokens, size_t *capacity, size_t *count, Token t) {
  ensure_capacity(tokens, capacity, *count, "add_token");
  (*tokens)[(*count)++] = t;
}

Token *tokenize(const char *sourceCode, size_t *tokenCount) {
  size_t capacity = 100;
  unsigned int line = 1;
  unsigned short int column = 1;
  Token *tokens = (Token *)malloc_safe(capacity * sizeof(Token), "tokenize");
  *tokenCount = 0;
  const char *src = sourceCode;

  while (*src) {
    if (isskippable(*src)) {
      if (*src == '\n') {
        line++;
        column = 0;
      }
      src++;
      column++;
      continue;
    }

    if (*src == '-' && *(src + 1) == '#') {
      while (*src && *src != '\n') { src++; column++; }
      continue;
    }

    /* 4-character tokens */
    if (*src == '_' && *(src + 1) == '>' && *(src + 2) == '>') {
      if (*(src + 3) == '@') {
        add_token(&tokens, &capacity, tokenCount, create_token("_>>@", ReturnSuccessTk, line, column));
        src += 4; column += 4; continue;
      } else if (*(src + 3) == '!') {
        add_token(&tokens, &capacity, tokenCount, create_token("_>>!", ReturnErrorTk, line, column));
        src += 4; column += 4; continue;
      } else {
        add_token(&tokens, &capacity, tokenCount, create_token("_>>", ReturnTk, line, column));
        src += 3; column += 3; continue;
      }
    }

    /* 3-character tokens */
    if (*src == '~' && *(src + 1) == '!' && *(src + 2) == '!') {
      add_token(&tokens, &capacity, tokenCount, create_token("~!!", BreakTk, line, column));
      src += 3; column += 3; continue;
    }
    if (*src == '_' && *(src + 1) == '_' && *(src + 2) == '>') {
      add_token(&tokens, &capacity, tokenCount, create_token("__>", ContinueTk, line, column));
      src += 3; column += 3; continue;
    }

    /* 2-character tokens */
    if (*src == '!' && *(src + 1) == '?') {
      add_token(&tokens, &capacity, tokenCount, create_token("!?", UnwrapTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '?' && *(src + 1) == '?') {
      add_token(&tokens, &capacity, tokenCount, create_token("??", MatchTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '=' && *(src + 1) == '>') {
      add_token(&tokens, &capacity, tokenCount, create_token("=>", FatArrowTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '~' && *(src + 1) == '>') {
      add_token(&tokens, &capacity, tokenCount, create_token("~>", ImportTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '|' && *(src + 1) == '{') {
      add_token(&tokens, &capacity, tokenCount, create_token("|{", OpenArenaTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '}' && *(src + 1) == '|') {
      add_token(&tokens, &capacity, tokenCount, create_token("}|", CloseArenaTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '|' && *(src + 1) == '>') {
      add_token(&tokens, &capacity, tokenCount, create_token("|>", OpenTableTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '<' && *(src + 1) == '|') {
      add_token(&tokens, &capacity, tokenCount, create_token("<|", CloseTableTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '-' && *(src + 1) == '>') {
      add_token(&tokens, &capacity, tokenCount, create_token("->", ArrowTk, line, column));
      src += 2; column += 2; continue;
    }
    if ((*src == '=' || *src == '!' || *src == '<' || *src == '>') && *(src + 1) == '=') {
      char op[3] = {*src, '=', '\0'};
      add_token(&tokens, &capacity, tokenCount, create_token(op, BinaryOperatorTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '|' && *(src + 1) == '|') {
      add_token(&tokens, &capacity, tokenCount, create_token("||", BinaryOperatorTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '&' && *(src + 1) == '&') {
      add_token(&tokens, &capacity, tokenCount, create_token("&&", BinaryOperatorTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '<' && *(src + 1) == '<') {
      add_token(&tokens, &capacity, tokenCount, create_token("<<", BinaryOperatorTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '>' && *(src + 1) == '>') {
      add_token(&tokens, &capacity, tokenCount, create_token(">>", BinaryOperatorTk, line, column));
      src += 2; column += 2; continue;
    }
    if (*src == '*' && *(src + 1) == '*') {
      add_token(&tokens, &capacity, tokenCount, create_token("**", BinaryOperatorTk, line, column));
      src += 2; column += 2; continue;
    }

    /* Single-character tokens */
    if (*src == '(') {
      add_token(&tokens, &capacity, tokenCount, create_token("(", OpenParenTk, line, column));
      src++; column++; continue;
    }
    if (*src == ')') {
      add_token(&tokens, &capacity, tokenCount, create_token(")", CloseParenTk, line, column));
      src++; column++; continue;
    }
    if (*src == '{') {
      add_token(&tokens, &capacity, tokenCount, create_token("{", OpenBraceTk, line, column));
      src++; column++; continue;
    }
    if (*src == '}') {
      add_token(&tokens, &capacity, tokenCount, create_token("}", CloseBraceTk, line, column));
      src++; column++; continue;
    }
    if (*src == '[') {
      add_token(&tokens, &capacity, tokenCount, create_token("[", OpenBracketTk, line, column));
      src++; column++; continue;
    }
    if (*src == ']') {
      add_token(&tokens, &capacity, tokenCount, create_token("]", CloseBracketTk, line, column));
      src++; column++; continue;
    }
    if (*src == ';') {
      add_token(&tokens, &capacity, tokenCount, create_token(";", SemiColonTk, line, column));
      src++; column++; continue;
    }
    if (*src == ',') {
      add_token(&tokens, &capacity, tokenCount, create_token(",", CommaTk, line, column));
      src++; column++; continue;
    }
    if (*src == '.') {
      add_token(&tokens, &capacity, tokenCount, create_token(".", DotTk, line, column));
      src++; column++; continue;
    }
    if (*src == '=') {
      add_token(&tokens, &capacity, tokenCount, create_token("=", EqualsTk, line, column));
      src++; column++; continue;
    }
    if (*src == '?') {
      add_token(&tokens, &capacity, tokenCount, create_token("?", IfTk, line, column));
      src++; column++; continue;
    }
    if (*src == ':') {
      add_token(&tokens, &capacity, tokenCount, create_token(":", ElseTk, line, column));
      src++; column++; continue;
    }
    if (*src == '#') {
      add_token(&tokens, &capacity, tokenCount, create_token("#", WhileTk, line, column));
      src++; column++; continue;
    }
    if (*src == '@') {
      add_token(&tokens, &capacity, tokenCount, create_token("@", ForTk, line, column));
      src++; column++; continue;
    }
    if (*src == '$') {
      add_token(&tokens, &capacity, tokenCount, create_token("$", FunctionTk, line, column));
      src++; column++; continue;
    }
    if (*src == '|') {
      add_token(&tokens, &capacity, tokenCount, create_token("|", BinaryOperatorTk, line, column));
      src++; column++; continue;
    }
    if (*src == '&') {
      add_token(&tokens, &capacity, tokenCount, handle_ampersand_token(&src, (int *)&line, &column));
      continue;
    }
    if (*src == '+' || *src == '-' || *src == '*' || *src == '/' || *src == '%' || *src == '^' || *src == '<' || *src == '>' || *src == '!') {
      char op[2] = {*src, '\0'};
      TokenType type = BinaryOperatorTk;
      if (*tokenCount == 0 ||
          tokens[*tokenCount - 1].type == BinaryOperatorTk ||
          tokens[*tokenCount - 1].type == OpenParenTk ||
          tokens[*tokenCount - 1].type == CommaTk ||
          tokens[*tokenCount - 1].type == EqualsTk) {
        type = UnaryOperatorTk;
      }
      add_token(&tokens, &capacity, tokenCount, create_token(op, type, line, column));
      src++; column++; continue;
    }

    /* Numbers */
    if (isint(*src)) {
      char num[1024] = {0};
      unsigned short int i = 0;
      int isFloat = 0;
      int start_col = column;
      while (isint(*src) || *src == '.') {
        if (*src == '.' && isFloat) error("Multiple decimal points in number");
        if (*src == '.') isFloat = 1;
        num[i++] = *src++;
        column++;
      }
      num[i] = '\0';
      add_token(&tokens, &capacity, tokenCount, create_token(num, NumberTk, line, start_col));
      continue;
    }

    /* Identifiers and Keywords */
    if (isalpha_custom(*src) || *src == '_') {
      char ident[256] = {0};
      unsigned int i = 0;
      int start_col = column;
      while (isalpha_custom(*src) || *src == '_' || isdigit(*src) || *src == '.') {
        ident[i++] = *src++;
        column++;
      }
      ident[i] = '\0';
      TokenType type = IdentifierTk;
      if (!strcmp(ident, "let")) type = LetTk;
      else if (!strcmp(ident, "true") || !strcmp(ident, "false")) type = BooleanLiteralTk;
      else if (!strcmp(ident, "nil")) type = NilTk;
      else if (!strcmp(ident, "as"))  type = AsTk;
      else if (strchr(ident, '.'))    type = IdentifierImportTk;

      add_token(&tokens, &capacity, tokenCount, create_token(ident, type, line, start_col));
      continue;
    }

    /* Strings */
    if (isquote(*src)) {
      char quote = *src++;
      int start_col = column++;
      const char *start = src;
      int escaped = 0;
      while (*src && (*src != quote || escaped)) {
        escaped = (*src == '\\' && !escaped);
        if (*src == '\n') { line++; column = 0; }
        src++; column++;
      }
      if (*src != quote) error("Unterminated string literal");
      long long len = src - start;
      char *val = malloc_safe(len + 1, "string_val");
      strncpy(val, start, len); val[len] = '\0';
      add_token(&tokens, &capacity, tokenCount, create_token(val, StringTk, line, start_col));
      free_safe(val);
      src++; column++;
      continue;
    }

    /* UTF-8 and fallback */
    int ulen = utf8_char_len(*src);
    if (ulen > 1) {
      char utf8[5] = {0};
      strncpy(utf8, src, ulen);
      add_token(&tokens, &capacity, tokenCount, create_token(utf8, IdentifierTk, line, column));
      src += ulen; column += ulen; continue;
    }

    fprintf(stderr, "Unrecognized character: %c (ASCII %d) at line %d column %d\n", *src, *src, line, column);
    exit(1);
  }

  add_token(&tokens, &capacity, tokenCount, create_token("EOF", EOFTk, line, column));
  return tokens;
}

void free_tokens(Token *tokens, int tokenCount) {
  for (size_t i = 0; i < (size_t)tokenCount; i++) free_safe(tokens[i].value);
  free_safe(tokens);
}
