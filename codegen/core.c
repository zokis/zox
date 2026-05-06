#include "../codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"

static void cg_emit_extern_block(Codegen *cg) {
    static const char *externs[] = {
        "MK_BOOL",
        "MK_NUMBER",
        "MK_STRING",
        "MK_LIST",
        "MK_DICT",
        "MK_NIL",
        "create_native_fn",
        "declare_var",
        "lookup_var",
        "assign_var",
        "list_append_val",
        "z_rt_get_env",
        "z_rt_bin_op",
        "zox_rt_unary_op",
        "zox_rt_import",
        "zox_rt_is_true",
        "zox_rt_call",
        "zox_rt_func_setup",
        "zox_rt_func_pop",
        "zox_rt_result_ok",
        "zox_rt_result_err",
        "zox_rt_unwrap_has_err",
        "zox_rt_unwrap_ok",
        "zox_rt_match_cond",
        "zox_rt_get_index",
        "zox_rt_get_slice",
        "zox_rt_list_set",
        "zox_rt_dict_get",
        "zox_rt_dict_set",
        "zox_rt_declare_type",
        "zox_rt_member_get",
        "zox_rt_member_set",
    };
    size_t i;

    for (i = 0; i < sizeof(externs) / sizeof(externs[0]); i++) {
        cg_emit(cg, "extern %s", externs[i]);
    }
}

Codegen *create_codegen(FILE *output) {
    Codegen *cg = malloc(sizeof(Codegen));
    cg->output = output;
    cg->label_count = 0;
    cg->const_count = 0;
    cg->stack_depth = 0;
    cg->loop_depth = 0;
    cg->in_function = 0;
    cg->function_return_label[0] = '\0';
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

void cg_emit_data_string(Codegen *cg, const char *label, const char *value) {
    const unsigned char *p = (const unsigned char *)(value ? value : "");
    int first = 1;

    fprintf(cg->output, "    %s: db ", label);
    while (*p) {
        if (!first) fprintf(cg->output, ", ");
        fprintf(cg->output, "%u", (unsigned int)*p);
        first = 0;
        p++;
    }
    if (!first) fprintf(cg->output, ", ");
    fprintf(cg->output, "0\n");
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
    cg_emit(cg, "  call %s", fn);
    if (needs_align) cg_emit(cg, "  add rsp, 8");
}

void cg_push_loop(Codegen *cg, const char *break_label, const char *continue_label) {
    if (cg->loop_depth >= 64) {
        fprintf(stderr, "Codegen error: loop nesting too deep\n");
        exit(1);
    }
    snprintf(cg->break_labels[cg->loop_depth], sizeof(cg->break_labels[cg->loop_depth]), "%s", break_label);
    snprintf(cg->continue_labels[cg->loop_depth], sizeof(cg->continue_labels[cg->loop_depth]), "%s", continue_label);
    cg->loop_depth++;
}

void cg_pop_loop(Codegen *cg) {
    if (cg->loop_depth > 0) cg->loop_depth--;
}

const char *cg_current_break_label(Codegen *cg) {
    if (cg->loop_depth == 0) return NULL;
    return cg->break_labels[cg->loop_depth - 1];
}

const char *cg_current_continue_label(Codegen *cg) {
    if (cg->loop_depth == 0) return NULL;
    return cg->continue_labels[cg->loop_depth - 1];
}

void codegen_program(Codegen *cg, Program *program) {
    cg_emit(cg, "; --- Zox Generated Assembly ---");
    cg_emit_extern_block(cg);
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
