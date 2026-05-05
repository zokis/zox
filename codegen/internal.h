#ifndef ZOX_CODEGEN_INTERNAL_H
#define ZOX_CODEGEN_INTERNAL_H

#include "../codegen.h"

void cg_emit(Codegen *cg, const char *fmt, ...);
void cg_push(Codegen *cg, const char *reg);
void cg_pop(Codegen *cg, const char *reg);
void cg_emit_call(Codegen *cg, const char *fn);

#endif
