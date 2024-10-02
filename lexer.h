#include <stddef.h>

#ifndef LEXER_H
#define LEXER_H

typedef enum {
  NumberTk,            // 0
  IdentifierTk,        // 1
  LetTk,               // 2
  BinaryOperatorTk,    // 3
  BooleanLiteralTk,    // 4
  NilTk,               // 5
  EqualsTk,            // 6
  OpenParenTk,         // 7
  CloseParenTk,        // 8
  SemiColonTk,         // 9
  IfTk,                // 10
  ElseTk,              // 11
  OpenBraceTk,         // 12
  CloseBraceTk,        // 13
  WhileTk,             // 14
  ForTk,               // 15
  StringTk,            // 16
  CommaTk,             // 17
  FunctionTk,          // 18
  OpenBracketTk,       // 19
  CloseBracketTk,      // 20
  ArrowTk,             // 21
  OpenTableTk,         // 22
  CloseTableTk,        // 23
  ImportTk,            // 24
  IdentifierImportTk,  // 25
  AsTk,                // 26
  DotTk,               // 27
  UnaryOperatorTk,     // 28
  EOFTk                // 29
} TokenType;

typedef struct {
  char *value;
  TokenType type;
  int line;
  short int column;
} Token;

Token *tokenize(const char *sourceCode, size_t *tokenCount);

Token create_token(const char *value, TokenType type, int line,
                   short int column);
void free_tokens(Token *tokens, int tokenCount);

#endif  // LEXER_H
