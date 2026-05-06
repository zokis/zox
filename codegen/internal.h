#ifndef ZOX_CODEGEN_INTERNAL_H
#define ZOX_CODEGEN_INTERNAL_H

#include "../codegen.h"

void cg_emit(Codegen *cg, const char *fmt, ...);
void cg_emit_data_string(Codegen *cg, const char *label, const char *value);
void cg_push(Codegen *cg, const char *reg);
void cg_pop(Codegen *cg, const char *reg);
void cg_emit_call(Codegen *cg, const char *fn);
void cg_push_loop(Codegen *cg, const char *break_label, const char *continue_label);
void cg_pop_loop(Codegen *cg);
const char *cg_current_break_label(Codegen *cg);
const char *cg_current_continue_label(Codegen *cg);

#endif
