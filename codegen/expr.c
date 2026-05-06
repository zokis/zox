#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"

void codegen_expr(Codegen *cg, Expr *expr) {
    if (!expr) return;

    switch (expr->stmt.kind) {
        case BooleanLiteralAst: {
            BooleanLiteral *bl = (BooleanLiteral *)expr;
            cg_emit(cg, "  mov rdi, %u", bl->value);
            cg_emit_call(cg, "MK_BOOL");
            break;
        }

        case NumericLiteralAst: {
            NumericLiteral *num = (NumericLiteral *)expr;
            char label[32];
            uint64_t bits;
            double value = num->value;

            sprintf(label, "c_n_%d", cg->const_count++);
            memcpy(&bits, &value, 8);
            cg_emit(cg, "  section .data");
            cg_emit(cg, "    %s: dq 0x%016llX", label, (unsigned long long)bits);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  movsd xmm0, [rel %s]", label);
            cg_emit_call(cg, "MK_NUMBER");
            break;
        }

        case UnaryExprAst: {
            UnaryExpr *unary = (UnaryExpr *)expr;
            char label[32];
            codegen_expr(cg, unary->expr);
            cg_emit(cg, "  mov rdi, rax");
            sprintf(label, "uop_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit_data_string(cg, label, unary->operator);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", label);
            cg_emit_call(cg, "zox_rt_unary_op");
            break;
        }

        case StringLiteralAst: {
            StringLiteral *str = (StringLiteral *)expr;
            char label[32];
            sprintf(label, "s_s_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit_data_string(cg, label, str->value);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rdi, [rel %s]", label);
            cg_emit_call(cg, "MK_STRING");
            break;
        }

        case ListLiteralAst: {
            ListLiteral *list = (ListLiteral *)expr;
            cg_emit(cg, "  mov rdi, %zu", list->element_count);
            cg_emit_call(cg, "MK_LIST");
            cg_push(cg, "rax");
            for (size_t i = 0; i < list->element_count; i++) {
                codegen_expr(cg, list->elements[i]);
                cg_pop(cg, "rdi");
                cg_push(cg, "rdi");
                cg_emit(cg, "  mov rsi, rax");
                cg_emit_call(cg, "list_append_val");
                cg_pop(cg, "rax");
                cg_push(cg, "rax");
            }
            cg_pop(cg, "rax");
            break;
        }

        case DictLiteralAst: {
            DictLiteral *dict = (DictLiteral *)expr;
            size_t capacity = dict->element_count > 0 ? dict->element_count * 2 : 1;
            cg_emit(cg, "  mov rdi, %zu", capacity);
            cg_emit_call(cg, "MK_DICT");
            cg_push(cg, "rax");
            for (size_t i = 0; i < dict->element_count; i++) {
                codegen_expr(cg, dict->keys[i]);
                cg_push(cg, "rax");
                codegen_expr(cg, dict->values[i]);
                cg_emit(cg, "  mov rdx, rax");
                cg_pop(cg, "rsi");
                cg_pop(cg, "rdi");
                cg_push(cg, "rdi");
                cg_emit_call(cg, "zox_rt_dict_set");
                cg_pop(cg, "rax");
                cg_push(cg, "rax");
            }
            cg_pop(cg, "rax");
            break;
        }

        case BinaryExprAst: {
            BinaryExpr *bin = (BinaryExpr *)expr;
            char label[32];

            codegen_expr(cg, bin->left);
            cg_push(cg, "rax");
            codegen_expr(cg, bin->right);
            cg_emit(cg, "  mov rsi, rax");
            cg_pop(cg, "rdi");
            sprintf(label, "op_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit_data_string(cg, label, bin->operator);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rdx, [rel %s]", label);
            cg_emit_call(cg, "z_rt_bin_op");
            break;
        }

        case IdentifierAst: {
            Identifier *id = (Identifier *)expr;
            char label[32];

            cg_emit_call(cg, "z_rt_get_env");
            cg_emit(cg, "  mov rdi, rax");
            sprintf(label, "id_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit_data_string(cg, label, id->symbol);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", label);
            cg_emit_call(cg, "lookup_var");
            break;
        }

        case ListIndexAst: {
            ListIndex *index = (ListIndex *)expr;
            codegen_expr(cg, index->list);
            cg_push(cg, "rax");
            codegen_expr(cg, index->start);
            if (!index->is_slice) {
                cg_emit(cg, "  mov rsi, rax");
                cg_pop(cg, "rdi");
                cg_emit_call(cg, "zox_rt_get_index");
                break;
            }
            cg_push(cg, "rax");
            if (index->end) {
                codegen_expr(cg, index->end);
            } else {
                cg_emit_call(cg, "MK_NIL");
            }
            cg_emit(cg, "  mov rdx, rax");
            cg_pop(cg, "rsi");
            cg_pop(cg, "rdi");
            cg_emit_call(cg, "zox_rt_get_slice");
            break;
        }

        case DictKeyAst: {
            DictKey *dict_key = (DictKey *)expr;
            codegen_expr(cg, dict_key->dict);
            cg_push(cg, "rax");
            codegen_expr(cg, dict_key->key);
            cg_emit(cg, "  mov rsi, rax");
            cg_pop(cg, "rdi");
            cg_emit_call(cg, "zox_rt_dict_get");
            break;
        }

        case MemberExprAst: {
            MemberExpr *member = (MemberExpr *)expr;
            char label[32];
            codegen_expr(cg, member->object);
            cg_emit(cg, "  mov rdi, rax");
            sprintf(label, "m_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit_data_string(cg, label, member->member);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", label);
            cg_emit_call(cg, "zox_rt_member_get");
            break;
        }

        case MatchAst: {
            MatchExpr *match = (MatchExpr *)expr;
            int match_id = cg->label_count++;
            int match_stack_depth;
            char end_label[64];
            sprintf(end_label, "Z_L_match_end_%d", match_id);

            codegen_expr(cg, match->target);
            cg_push(cg, "rax");
            match_stack_depth = cg->stack_depth;

            for (size_t i = 0; i < match->case_count; i++) {
                MatchCase *c = match->cases[i];
                char next_label[64];
                sprintf(next_label, "Z_L_match_next_%d_%zu", match_id, i);
                cg->stack_depth = match_stack_depth;

                if (c->condition != NULL) {
                    codegen_expr(cg, c->condition);
                    cg_emit(cg, "  mov rsi, rax");
                    cg_emit(cg, "  mov rdi, [rsp]");
                    cg_emit_call(cg, "zox_rt_match_cond");
                    cg_emit(cg, "  cmp eax, 0");
                    cg_emit(cg, "  je %s", next_label);
                }

                codegen_expr(cg, c->branch);
                cg_emit(cg, "  add rsp, 8");
                cg->stack_depth = match_stack_depth - 1;
                cg_emit(cg, "  jmp %s", end_label);
                cg_emit(cg, "%s:", next_label);
            }

            cg->stack_depth = match_stack_depth;
            cg_emit(cg, "  add rsp, 8");
            cg->stack_depth = match_stack_depth - 1;
            cg_emit_call(cg, "MK_NIL");
            cg_emit(cg, "%s:", end_label);
            break;
        }

        case UnwrapAst: {
            UnwrapExpr *unwrap = (UnwrapExpr *)expr;
            int unwrap_id = cg->label_count++;
            int unwrap_stack_depth;
            char ok_label[64];

            if (!cg->in_function) {
                cg_emit(cg, "  ; unwrap outside function is unsupported in compiled mode");
                cg_emit(cg, "  ud2");
                break;
            }

            sprintf(ok_label, "Z_L_unwrap_ok_%d", unwrap_id);
            codegen_expr(cg, unwrap->expr);
            cg_push(cg, "rax");
            unwrap_stack_depth = cg->stack_depth;
            cg_emit(cg, "  mov rdi, [rsp]");
            cg_emit_call(cg, "zox_rt_unwrap_has_err");
            cg_emit(cg, "  cmp eax, 0");
            cg_emit(cg, "  je %s", ok_label);
            cg_emit(cg, "  pop rax");
            cg->stack_depth = unwrap_stack_depth - 1;
            cg_emit(cg, "  jmp %s", cg->function_return_label);
            cg_emit(cg, "%s:", ok_label);
            cg->stack_depth = unwrap_stack_depth;
            cg_emit(cg, "  mov rdi, [rsp]");
            cg_emit_call(cg, "zox_rt_unwrap_ok");
            cg_emit(cg, "  add rsp, 8");
            cg->stack_depth = unwrap_stack_depth - 1;
            break;
        }

        case CallExprAst: {
            CallExpr *call = (CallExpr *)expr;
            codegen_expr(cg, call->callee);
            cg_push(cg, "rax ; Save callee");
            for (int i = (int)call->arg_count - 1; i >= 0; i--) {
                codegen_expr(cg, call->arguments[i]);
                cg_push(cg, "rax");
            }
            cg_emit(cg, "  mov rsi, rsp");
            cg_emit(cg, "  mov rdx, %zu", call->arg_count);
            cg_emit(cg, "  mov rdi, [rsp + %zu]", call->arg_count * 8);
            cg_emit_call(cg, "zox_rt_call");
            cg_emit(cg, "  add rsp, %zu", (call->arg_count + 1) * 8);
            cg->stack_depth -= (int)(call->arg_count + 1);
            break;
        }

        case NilAst:
            cg_emit_call(cg, "MK_NIL");
            break;

        case AssignVarAst: {
            AssignVar *assign = (AssignVar *)expr;
            char label[32];

            codegen_expr(cg, assign->value);
            cg_push(cg, "rax");
            cg_emit_call(cg, "z_rt_get_env");
            cg_emit(cg, "  mov rdi, rax");
            sprintf(label, "av_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit_data_string(cg, label, assign->varname);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", label);
            cg_pop(cg, "rdx");
            cg_emit_call(cg, "assign_var");
            break;
        }

        case AssignListExprAst: {
            AssignListExpr *assign = (AssignListExpr *)expr;
            codegen_expr(cg, assign->target);
            cg_push(cg, "rax");
            codegen_expr(cg, assign->index);
            cg_push(cg, "rax");
            codegen_expr(cg, assign->value);
            cg_emit(cg, "  mov rdx, rax");
            cg_pop(cg, "rsi");
            cg_pop(cg, "rdi");
            cg_emit_call(cg, "zox_rt_list_set");
            break;
        }

        case AssignDictExprAst: {
            AssignDictExpr *assign = (AssignDictExpr *)expr;
            codegen_expr(cg, assign->target);
            cg_push(cg, "rax");
            codegen_expr(cg, assign->key);
            cg_push(cg, "rax");
            codegen_expr(cg, assign->value);
            cg_emit(cg, "  mov rdx, rax");
            cg_pop(cg, "rsi");
            cg_pop(cg, "rdi");
            cg_emit_call(cg, "zox_rt_dict_set");
            break;
        }

        case AssignMemberExprAst: {
            AssignMemberExpr *assign = (AssignMemberExpr *)expr;
            char label[32];
            codegen_expr(cg, assign->object);
            cg_push(cg, "rax");
            codegen_expr(cg, assign->value);
            cg_emit(cg, "  mov rdx, rax");
            cg_pop(cg, "rdi");
            sprintf(label, "am_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit_data_string(cg, label, assign->member);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", label);
            cg_emit_call(cg, "zox_rt_member_set");
            break;
        }

        default:
            break;
    }
}
