#ifndef ZOX_CODEGEN_H
#define ZOX_CODEGEN_H

#include "ast.h"
#include <stdio.h>

typedef struct {
    FILE *output;
    int label_count;
    int const_count;
    int stack_depth;
} Codegen;

Codegen *create_codegen(FILE *output);
void free_codegen(Codegen *cg);

void codegen_program(Codegen *cg, Program *program);
void codegen_stmt(Codegen *cg, Stmt *stmt);
void codegen_expr(Codegen *cg, Expr *expr);

#endif
