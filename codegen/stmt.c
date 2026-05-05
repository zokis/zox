#include <stdio.h>

#include "internal.h"

void codegen_stmt(Codegen *cg, Stmt *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case ImportAst: {
            ImportStmt *import = (ImportStmt *)stmt;
            char names_label[32], aliases_label[32], mod_name_label[32];
            sprintf(names_label, "i_n_%d", cg->const_count);
            sprintf(aliases_label, "i_a_%d", cg->const_count);
            sprintf(mod_name_label, "m_n_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit(cg, "    %s: db \"%s\", 0", mod_name_label, import->module_name);
            for (size_t i = 0; i < import->import_count; i++) {
                char item_label[32];
                sprintf(item_label, "i_i_%d_%zu", cg->const_count, i);
                cg_emit(cg, "    %s: db \"%s\", 0", item_label, import->imports[i]->name);
            }
            cg_emit(cg, "    %s_ptr: ", names_label);
            for (size_t i = 0; i < import->import_count; i++) {
                cg_emit(cg, "      dq i_i_%d_%zu", cg->const_count, i);
            }
            cg_emit(cg, "    %s_ptr: ", aliases_label);
            for (size_t i = 0; i < import->import_count; i++) {
                cg_emit(cg, "      dq 0");
            }
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rdi, [rel %s]", mod_name_label);
            cg_emit(cg, "  lea rsi, [rel %s_ptr]", names_label);
            cg_emit(cg, "  lea rdx, [rel %s_ptr]", aliases_label);
            cg_emit(cg, "  mov rcx, %zu", import->import_count);
            cg_emit_call(cg, "zox_rt_import");
            break;
        }

        case VarDeclarationAst: {
            VarDeclaration *var = (VarDeclaration *)stmt;
            codegen_expr(cg, var->value);
            cg_push(cg, "rax");
            cg_emit_call(cg, "z_rt_get_env");
            cg_emit(cg, "  mov rdi, rax");
            cg_emit(cg, "  section .data");
            char var_label[32];
            sprintf(var_label, "v_n_%d", cg->const_count++);
            cg_emit(cg, "    %s: db \"%s\", 0", var_label, var->varname);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", var_label);
            cg_pop(cg, "rdx");
            cg_emit_call(cg, "declare_var");
            break;
        }

        case AssignListVarAst: {
            AssignListVar *assign = (AssignListVar *)stmt;
            cg_emit_call(cg, "z_rt_get_env");
            cg_emit(cg, "  mov rdi, rax");
            char label[32];
            sprintf(label, "alv_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit(cg, "    %s: db \"%s\", 0", label, assign->varname);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", label);
            cg_emit_call(cg, "lookup_var");
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

        case AssignDictVarAst: {
            AssignDictVar *assign = (AssignDictVar *)stmt;
            cg_emit_call(cg, "z_rt_get_env");
            cg_emit(cg, "  mov rdi, rax");
            char label[32];
            sprintf(label, "adv_%d", cg->const_count++);
            cg_emit(cg, "  section .data");
            cg_emit(cg, "    %s: db \"%s\", 0", label, assign->varname);
            cg_emit(cg, "  section .text");
            cg_emit(cg, "  lea rsi, [rel %s]", label);
            cg_emit_call(cg, "lookup_var");
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

        case IfAst: {
            IfExpr *if_expr = (IfExpr *)stmt;
            int label_id = cg->label_count++;
            char else_label[64], end_label[64];
            sprintf(else_label, "Z_L_if_else_%d", label_id);
            sprintf(end_label, "Z_L_if_end_%d", label_id);
            codegen_expr(cg, if_expr->condition);
            cg_emit(cg, "  mov rdi, rax");
            cg_emit_call(cg, "zox_rt_is_true");
            cg_emit(cg, "  cmp eax, 0");
            cg_emit(cg, "  je %s", else_label);
            for (size_t i = 0; i < if_expr->body_count; i++) {
                codegen_stmt(cg, if_expr->body[i]);
            }
            cg_emit(cg, "  jmp %s", end_label);
            cg_emit(cg, "%s:", else_label);
            if (if_expr->else_if) {
                codegen_stmt(cg, (Stmt *)if_expr->else_if);
            } else if (if_expr->else_body_count > 0) {
                for (size_t i = 0; i < if_expr->else_body_count; i++) {
                    codegen_stmt(cg, if_expr->else_body[i]);
                }
            }
            cg_emit(cg, "%s:", end_label);
            break;
        }

        case FuncDefAst: {
            FuncDef *func = (FuncDef *)stmt;
            int func_id = cg->label_count++;
            char func_label[64], skip_label[64], params_label[64];
            sprintf(func_label, "Z_F_%s_%d", func->name, func_id);
            sprintf(skip_label, "Z_F_skip_%d", func_id);
            sprintf(params_label, "Z_F_params_%d", func_id);
            cg_emit(cg, "  jmp %s", skip_label);
            cg_emit(cg, "  section .data");
            for (size_t i = 0; i < func->param_count; i++) {
                cg_emit(cg, "    %s_p%zu: db \"%s\", 0", params_label, i, func->params[i]);
            }
            cg_emit(cg, "    %s_ptr: ", params_label);
            for (size_t i = 0; i < func->param_count; i++) {
                cg_emit(cg, "      dq %s_p%zu", params_label, i);
            }
            cg_emit(cg, "  section .text");
            cg_emit(cg, "%s:", func_label);
            cg_emit(cg, "  push rbp");
            cg_emit(cg, "  mov rbp, rsp");

            int old_depth = cg->stack_depth;
            cg->stack_depth = 0;

            cg_push(cg, "rdi ; parent_env");
            cg_push(cg, "rsi ; args_array");
            cg_push(cg, "rdx ; arg_count");
            cg_emit(cg, "  mov r8, rdx");
            cg_emit(cg, "  mov rcx, rsi");
            cg_emit(cg, "  lea rsi, [rel %s_ptr]", params_label);
            cg_emit(cg, "  mov rdx, %zu", func->param_count);
            cg_emit_call(cg, "zox_rt_func_setup");
            cg_pop(cg, "rdi");
            cg_pop(cg, "rsi");
            cg_pop(cg, "rdx");

            for (size_t i = 0; i < func->body_count; i++) {
                codegen_stmt(cg, func->body[i]);
            }

            cg_emit_call(cg, "MK_NIL");
            cg_push(cg, "rax");
            cg_emit_call(cg, "zox_rt_func_pop");
            cg_pop(cg, "rax");
            cg_emit(cg, "  pop rbp");
            cg_emit(cg, "  ret");

            cg_emit(cg, "%s:", skip_label);
            cg_emit_call(cg, "z_rt_get_env");
            cg_push(cg, "rax");
            cg_emit(cg, "  lea rdi, [rel %s_ptr]", params_label);
            cg_emit(cg, "  mov rsi, %zu", func->param_count);
            cg_emit(cg, "  lea rdx, [rel %s]", func_label);
            cg_emit_call(cg, "create_native_fn");
            cg_push(cg, "rax");
            cg_emit(cg, "  section .data");
            char name_label[64];
            sprintf(name_label, "f_n_%d", cg->const_count++);
            cg_emit(cg, "    %s: db \"%s\", 0", name_label, func->name);
            cg_emit(cg, "  section .text");
            cg_pop(cg, "rdx");
            cg_emit(cg, "  lea rsi, [rel %s]", name_label);
            cg_pop(cg, "rdi");
            cg_emit_call(cg, "declare_var");
            cg->stack_depth = old_depth;
            break;
        }

        case ReturnAst: {
            ReturnStmt *ret = (ReturnStmt *)stmt;
            if (ret->value) codegen_expr(cg, ret->value);
            else cg_emit_call(cg, "MK_NIL");
            cg_push(cg, "rax");
            cg_emit_call(cg, "zox_rt_func_pop");
            cg_pop(cg, "rax");
            cg_emit(cg, "  mov rsp, rbp");
            cg_emit(cg, "  pop rbp");
            cg_emit(cg, "  ret");
            break;
        }

        case WhileAst: {
            WhileExpr *while_expr = (WhileExpr *)stmt;
            int label_id = cg->label_count++;
            char start_label[64], end_label[64];
            sprintf(start_label, "Z_L_while_s_%d", label_id);
            sprintf(end_label, "Z_L_while_e_%d", label_id);
            cg_emit(cg, "%s:", start_label);
            codegen_expr(cg, while_expr->condition);
            cg_emit(cg, "  mov rdi, rax");
            cg_emit_call(cg, "zox_rt_is_true");
            cg_emit(cg, "  cmp eax, 0");
            cg_emit(cg, "  je %s", end_label);
            for (size_t i = 0; i < while_expr->body_count; i++) codegen_stmt(cg, while_expr->body[i]);
            cg_emit(cg, "  jmp %s", start_label);
            cg_emit(cg, "%s:", end_label);
            break;
        }

        case ForAst: {
            ForExpr *for_expr = (ForExpr *)stmt;
            int label_id = cg->label_count++;
            char start_label[64], end_label[64];
            sprintf(start_label, "Z_L_for_s_%d", label_id);
            sprintf(end_label, "Z_L_for_e_%d", label_id);
            if (for_expr->initialization) codegen_stmt(cg, (Stmt *)for_expr->initialization);
            cg_emit(cg, "%s:", start_label);
            if (for_expr->condition) {
                codegen_expr(cg, for_expr->condition);
                cg_emit(cg, "  mov rdi, rax");
                cg_emit_call(cg, "zox_rt_is_true");
                cg_emit(cg, "  cmp eax, 0");
                cg_emit(cg, "  je %s", end_label);
            }
            for (size_t i = 0; i < for_expr->body_count; i++) codegen_stmt(cg, for_expr->body[i]);
            if (for_expr->increment) codegen_expr(cg, for_expr->increment);
            cg_emit(cg, "  jmp %s", start_label);
            cg_emit(cg, "%s:", end_label);
            break;
        }

        default:
            codegen_expr(cg, (Expr *)stmt);
            break;
    }
}
