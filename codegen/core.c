#include "../codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "internal.h"

Codegen *create_codegen(FILE *output) {
    Codegen *cg = malloc(sizeof(Codegen));
    cg->output = output;
    cg->label_count = 0;
    cg->const_count = 0;
    cg->stack_depth = 0;
    return cg;
}

void free_codegen(Codegen *cg) {
    free(cg);
}

void cg_emit(Codegen *cg, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(cg->output, fmt, args);
    va_end(args);
    fprintf(cg->output, "\n");
}

void cg_push(Codegen *cg, const char *reg) {
    cg_emit(cg, "  push %s", reg);
    cg->stack_depth++;
}

void cg_pop(Codegen *cg, const char *reg) {
    cg_emit(cg, "  pop %s", reg);
    cg->stack_depth--;
}

void cg_emit_call(Codegen *cg, const char *fn) {
    int needs_align = (cg->stack_depth % 2 != 0);
    if (needs_align) cg_emit(cg, "  sub rsp, 8");
    cg_emit(cg, "  extern %s", fn);
    cg_emit(cg, "  call %s", fn);
    if (needs_align) cg_emit(cg, "  add rsp, 8");
}

void codegen_program(Codegen *cg, Program *program) {
    cg_emit(cg, "; --- Zox Generated Assembly ---");
    cg_emit(cg, "section .data");
    cg_emit(cg, "  fmt_num: db \"%%g\", 10, 0");
    cg_emit(cg, "section .text");
    cg_emit(cg, "global zox_main");
    cg_emit(cg, "zox_main:");
    cg_emit(cg, "  push rbp");
    cg_emit(cg, "  mov rbp, rsp");
    cg->stack_depth = 0;

    for (size_t i = 0; i < program->body_count; i++) {
        codegen_stmt(cg, program->body[i]);
    }

    cg_emit(cg, "  mov rax, 0");
    cg_emit(cg, "  pop rbp");
    cg_emit(cg, "  ret");
}
