#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"
#include "malloc_safe.h"

Token create_token(const char *value, TokenType type, int line, short int column) {
    Token t;
    t.value = strdup(value);
    t.type = type;
    t.line = line;
    t.column = column;
    return t;
}

int isalpha_custom(char c) {
   return isalpha(c) || (unsigned char)c >= 128; // Extend to include UTF-8 characters
}

int isskippable(char c) {
    return c == ' ' || c == '\n' || c == '\t' || '\r';
}

int isint(char c) {
    return isdigit(c);
}

int isquote(char c) {
    return c == '"' || c == '\'';
}

void ensure_capacity(Token **tokens, size_t *capacity, size_t tokenCount, const char *errMsg) {
    if (tokenCount >= *capacity) {
        *capacity *= 2;
        *tokens = realloc_safe(*tokens, *capacity * sizeof(Token), errMsg);
    }
}


int utf8_char_len(char c) {
    if ((c & 0x80) == 0) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    return 1;  // Invalid UTF-8 character
}

Token *tokenize(const char *sourceCode, size_t *tokenCount) {
    size_t capacity = 10;
    unsigned int line = 1;
    unsigned short int column = 1;
    Token *tokens = malloc_safe(capacity * sizeof(Token), "tokenize");
    *tokenCount = 0;
    const char *src = sourceCode;
    while (*src) {
        unsigned long long int char_len = utf8_char_len(*src);
        if (*src == '-' && *(src + 1) == '#') {
            while (*src && *src != '\n') {
                src++;
                column++;
            }
            if (*src == '\n') {
                line++;
                column = 1;
                src++;
            }
        } else if (*src == '~' && *(src + 1) == '>') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'ImportTk'");
            tokens[(*tokenCount)++] = create_token("~>", ImportTk, line, column);
            src += 2;
            column += 2;
            while (isspace(*src)) { src++; column++; }
            if (isalpha(*src) || *src == '_'  || *src == '.') {
                char ident[256] = {0};
                unsigned int i = 0;
                while (isalnum(*src) || *src == '_' || *src == '.') {
                    ident[i++] = *src++;
                    column++;
                }
                ident[i] = '\0';
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'IdentifierImportTk'");
                tokens[(*tokenCount)++] = create_token(strdup(ident), IdentifierImportTk, line, column);
            }
        } else if (*src == 'a' && *(src + 1) == 's') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'AsTk'");
            tokens[(*tokenCount)++] = create_token("as", AsTk, line, column);
            src += 2;
            column += 2;
        } else if (*src == '.') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'DotTk'");
            tokens[(*tokenCount)++] = create_token(".", DotTk, line, column);
            src++;
            column++;
        } else if (*src == '#') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'WhileTk'");
            tokens[(*tokenCount)++] = create_token("#", WhileTk, line, column);
            src++;
            column++;
        } else if (*src == ',') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'CommaTk'");
            tokens[(*tokenCount)++] = create_token(",", CommaTk, line, column);
            src++;
            column++;
        } else if (*src == '$') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'FunctionTk'");
            tokens[(*tokenCount)++] = create_token("$", FunctionTk, line, column);
            src++;
            column++;
        } else if (*src == '@') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'ForTk'");
            tokens[(*tokenCount)++] = create_token("@", ForTk, line, column);
            src++;
            column++;
        } else if (*src == '?') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'IfTk'");
            tokens[(*tokenCount)++] = create_token("?", IfTk, line, column);
            src++;
            column++;
        } else if (*src == ':') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'ElseTk'");
            tokens[(*tokenCount)++] = create_token(":", ElseTk, line, column);
            src++;
            column++;
        } else if (*src == '|' && *(src + 1) == '>') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'TableTk'");
            tokens[(*tokenCount)++] = create_token("|>", OpenTableTk, line, column);
            src += 2;
            column += 2;
        } else if (*src == '<' && *(src + 1) == '|') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'TableTk'");
            tokens[(*tokenCount)++] = create_token("<|", CloseTableTk, line, column);
            src += 2;
            column += 2;
        } else if (*src == '(') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'OpenParenTk'");
            tokens[(*tokenCount)++] = create_token("(", OpenParenTk, line, column);
            src++;
            column++;
        } else if (*src == ')') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'CloseParenTk'");
            tokens[(*tokenCount)++] = create_token(")", CloseParenTk, line, column);
            src++;
            column++;
        } else if (*src == '{') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'OpenBraceTk'");
            tokens[(*tokenCount)++] = create_token("{", OpenBraceTk, line, column);
            src++;
            column++;
        } else if (*src == '}') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'CloseBraceTk'");
            tokens[(*tokenCount)++] = create_token("}", CloseBraceTk, line, column);
            src++;
            column++;
        } else if (*src == '[') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'OpenBracketTk'");
            tokens[(*tokenCount)++] = create_token("[", OpenBracketTk, line, column);
            src++;
            column++;
        } else if (*src == ']') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'CloseBracketTk'");
            tokens[(*tokenCount)++] = create_token("]", CloseBracketTk, line, column);
            src++;
            column++;
        } else if (*src == '>' || *src == '<' || *src == '=' || *src == '!') {
            char op[3] = {0};
            unsigned int i = 0;
            while (*src == '>' || *src == '<' || *src == '=' || *src == '!') {
                op[i++] = *src++;
                column++;
            }
            op[i] = '\0';
            if (op[0] == '=' && i == 1) {
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'EqualsTk'");
                tokens[(*tokenCount)++] = create_token("=", EqualsTk, line, column);
            } else {
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
                tokens[(*tokenCount)++] = create_token(op, BinaryOperatorTk, line, column);
            }
        } else if (*src == '&') {
            if (*(src + 1) == '&') {
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
                tokens[(*tokenCount)++] = create_token("&&", BinaryOperatorTk, line, column);
                src += 2;
                column += 2;
            } else {
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
                tokens[(*tokenCount)++] = create_token("&", BinaryOperatorTk, line, column);
                src++;
                column++;
            }
        } else if (*src == '|') {
            if (*(src + 1) == '|') {
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
                tokens[(*tokenCount)++] = create_token("||", BinaryOperatorTk, line, column);
                src += 2;
                column += 2;
            } else {
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
                tokens[(*tokenCount)++] = create_token("|", BinaryOperatorTk, line, column);
                src++;
                column++;
            }
        } else if (*src == '^') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
            tokens[(*tokenCount)++] = create_token("^", BinaryOperatorTk, line, column);
            src++;
            column++;
        } else if (*src == '<' && *(src + 1) == '<') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
            tokens[(*tokenCount)++] = create_token("<<", BinaryOperatorTk, line, column);
            src += 2;
            column += 2;
        } else if (*src == '>' && *(src + 1) == '>') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
            tokens[(*tokenCount)++] = create_token(">>", BinaryOperatorTk, line, column);
            src += 2;
            column += 2;
        } else if (*src == '*' && *(src + 1) == '*') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
            tokens[(*tokenCount)++] = create_token("**", BinaryOperatorTk, line, column);
            src += 2;
            column += 2;
        } else if (*src == '%') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
            tokens[(*tokenCount)++] = create_token("%", BinaryOperatorTk, line, column);
            src++;
            column++;
        } else if (isint(*src) || (*src == '-' && isint(*(src + 1)))) {
            char num[1024] = {0};
            unsigned short int i = 0;
            unsigned short int isFloat = 0;
            if (*src == '-') {
                num[i++] = *src++;
                column++;
            }
            while (isint(*src) || *src == '.') {
                if (*src == '.' && isFloat) {
                    free_tokens(tokens, *tokenCount);
                    fprintf(stderr, "A floating point number can only have one decimal point; line %i column %i\n", line, column);
                    exit(1);
                } else if (*src == '.') {
                    isFloat = 1;
                }
                num[i++] = *src++;
                column++;
            }
            num[i] = '\0';
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'NumberTk'");
            tokens[(*tokenCount)++] = create_token(num, NumberTk, line, column);
        } else if (*src == '-' && *(src + 1) == '>') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'ArrowTk'");
            tokens[(*tokenCount)++] = create_token("->", ArrowTk, line, column);
            src += 2;
            column += 2;
        } else if (*src == '+' || *src == '-' || *src == '*' || *src == '/') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'BinaryOperatorTk'");
            char op[2] = {*src, '\0'};
            tokens[(*tokenCount)++] = create_token(op, BinaryOperatorTk, line, column);
            src++;
            column++;
        } else if (*src == ';') {
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'SemiColonTk'");
            tokens[(*tokenCount)++] = create_token(";", SemiColonTk, line, column);
            src++;
            column++;
        } else if (isalpha_custom(*src) || *src == '.') {
            char ident[256] = {0};
            unsigned int i = 0;
            while (isalpha_custom(*src) || *src == '.' || isdigit(*src)) {
                ident[i++] = *src++;
                column++;
            }
            ident[i] = '\0';
            TokenType reserved = IdentifierImportTk;
            if (!strcmp(ident, "let")) {
                reserved = LetTk;
            } else if (!strcmp(ident, "true") || !strcmp(ident, "false")) {
                reserved = BooleanLiteralTk;
            } else if (!strcmp(ident, "nil")) {
                reserved = NilTk;
            } else if (!strchr(ident, '.')) {
                reserved = IdentifierTk;
            }
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'IdentifierTk'");
            tokens[(*tokenCount)++] = create_token(ident, reserved, line, column);
        } else if (isquote(*src)) {
            char quote_type = *src;
            src++;
            column++;
            const char *start = src;
            while (*src && *src != quote_type) {
                src++;
                column++;
            }
            if (*src != quote_type) {
                fprintf(stderr, "Unterminated string literal\n");
                exit(1);
            }
            long long int length = src - start;
            char *str_value = malloc_safe(length + 1, "tokenize 'str_value:StringTk'");
            strncpy(str_value, start, length);
            str_value[length] = '\0';
            ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'StringTk'");
            tokens[(*tokenCount)++] = create_token(str_value, StringTk, line, column);
            free_safe(str_value);
            src++;
            column++;
        } else if (isskippable(*src)) {
            if (*src == '\n') {
                line++;
                column = 0;
            }
            src++;
            column++;
        }  else {
            // Handle UTF-8 characters
            if (char_len > 1) {
                char utf8_char[5] = {0};  // UTF-8 characters can be up to 4 bytes
                strncpy(utf8_char, src, char_len);
                ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'UTF8Char'");
                tokens[(*tokenCount)++] = create_token(utf8_char, IdentifierTk, line, column);
                src += char_len;
                column += char_len;
            } else {
                free_tokens(tokens, *tokenCount);
                fprintf(stderr, "Unrecognized character found in source: %c (ASCII: %d)\n", *src, *src);
                exit(1);
            }
        }
    }
    ensure_capacity(&tokens, &capacity, *tokenCount, "tokenize 'EOFTk'");
    tokens[(*tokenCount)++] = create_token("EOF", EOFTk, line, column);
    return tokens;
}

void free_tokens(Token *tokens, int tokenCount) {
    for (size_t i = 0; i < tokenCount; i++) {
        free_safe(tokens[i].value);
    }
    free_safe(tokens);
}
