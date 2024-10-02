#ifndef GLOBAL_H
#define GLOBAL_H

#include <stddef.h>
#include <stdio.h>
#include <stddef.h>
#include <setjmp.h>

#include "lexer.h"
#include "values.h"
typedef struct {
  int is_repl;
  jmp_buf error_jmp;
} ExecutionContext;

extern ExecutionContext global_context;

ssize_t getline(char **lineptr, size_t *n, FILE *stream);
void parser_error(const char *message, Token *token, TokenType type);
void error(const char *message);
char *read_file(const char *filename);
unsigned short int compare_runtimeval(RuntimeVal *a, RuntimeVal *b);
unsigned short int contains(RuntimeVal *obj, RuntimeVal *value);
ListVal *dict_to_keys(DictVal *dict);

#endif  // GLOBAL_H