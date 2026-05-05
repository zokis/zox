#include "ast.h"
#include "builtins.h"
#include "env.h"
#include "eval.h"
#include "global.h"
#include "values.h"
#include "zox_alloc.h"
#include "parser.h"
#include "eval/eval_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

ExecutionContext global_context = {0};
Environment *env_stack[4096];
int env_stack_ptr = 0;

void push_env(Environment *env) {
    if (env_stack_ptr < 4096) env_stack[env_stack_ptr++] = env;
    else { fprintf(stderr, "Error: Env stack overflow\n"); exit(1); }
}

void pop_env() {
    if (env_stack_ptr > 0) env_stack_ptr--;
}

Environment *get_current_env() {
    if (env_stack_ptr > 0) return env_stack[env_stack_ptr - 1];
    return builtins_env;
}

Environment *z_rt_get_env() {
    return get_current_env();
}

Environment *zox_rt_func_setup(Environment *parent_env, const char **params, size_t param_count, RuntimeVal **args, size_t arg_count) {
    if (!parent_env) parent_env = builtins_env;
    Environment *func_env = create_environment(parent_env, "func_env");
    if (param_count != arg_count) {
        fprintf(stderr, "Error: Function argument count mismatch. Expected %zu, got %zu\n", param_count, arg_count);
        exit(1);
    }
    for (size_t i = 0; i < param_count; i++) {
        declare_var(func_env, params[i], args[i]);
    }
    push_env(func_env);
    return func_env;
}

void zox_rt_func_pop() {
    pop_env();
}

void zox_rt_init() {
    Environment *env = create_environment(NULL, "global");
    builtins_env = env;
    env_stack_ptr = 0;
    push_env(env);
    register_builtins(env);
    declare_var(env, "nil",   (RuntimeVal *)MK_NIL());
    declare_var(env, "true",  (RuntimeVal *)MK_BOOL(1));
    declare_var(env, "false", (RuntimeVal *)MK_BOOL(0));
    declare_var(env, "PI",    (RuntimeVal *)MK_NUMBER(3.14159265359));
}

void zox_rt_cleanup() {
    break_env_cycles(builtins_env);
    zox_alloc_cleanup();
    zox_arena_destroy();
}

void run_program(Program *program, Environment *env) {
    RuntimeVal *result = eval_program(program, env);
    release(result);
}

extern void zox_main();

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    zox_rt_init();
    zox_main();
    zox_rt_cleanup();
    return 0;
}

RuntimeVal *zox_rt_call(RuntimeVal *callee, RuntimeVal **args, size_t arg_count) {
    if (!callee || callee->type != FUNCTION_T) {
        fprintf(stderr, "Error: Attempted to call a non-function value (type %d).\n", callee ? callee->type : -1);
        exit(1);
    }
    FunctionVal *func = (FunctionVal *)callee;
    return zox_call_function(func, get_current_env(), args, arg_count);
}

int zox_rt_is_true(RuntimeVal *val) {
    if (!val) return 0;
    if (val->type != BOOLEAN_T) {
        fprintf(stderr, "Error: Condition must be a boolean value.\n");
        exit(1);
    }
    return ((BooleanVal *)val)->value;
}

void zox_rt_import(const char *module_name, const char **names, const char **aliases, size_t count) {
    if (strcmp(module_name, "string") == 0) {
        extern void init_string_module(Environment *env);
        Environment *mod_env = create_environment(NULL, "string");
        init_string_module(mod_env);
        for (size_t i = 0; i < count; i++) {
            RuntimeVal *val = lookup_var(mod_env, names[i]);
            if (val) {
                declare_var(builtins_env, aliases[i] ? aliases[i] : names[i], val);
            }
        }
        release_env(mod_env);
    }
}

// Wrapper for binary expressions to ensure correct pointer return
RuntimeVal *z_rt_bin_op(RuntimeVal *lhs, RuntimeVal *rhs, const char *op) {
    return eval_binary_expr_evaluated(lhs, rhs, op);
}

RuntimeVal *zox_rt_list_get(RuntimeVal *list_val, RuntimeVal *index_val) {
    if (!list_val || list_val->type != LIST_T) {
        fprintf(stderr, "Error: Attempted to index a non-list value.\n");
        exit(1);
    }
    if (!index_val || index_val->type != NUMBER_T) {
        fprintf(stderr, "Error: List index must be a number.\n");
        exit(1);
    }

    ListVal *list = (ListVal *)list_val;
    int idx = (int)((NumberVal *)index_val)->value;
    if (idx < 0) idx = (int)list->size + idx;
    if (idx < 0 || idx >= (int)list->size) {
        fprintf(stderr, "Error: List index out of bounds.\n");
        exit(1);
    }

    RuntimeVal *result = list->items[idx];
    retain(result);
    return result;
}

RuntimeVal *zox_rt_list_slice(RuntimeVal *list_val, RuntimeVal *start_val, RuntimeVal *end_val) {
    if (!list_val || list_val->type != LIST_T) {
        fprintf(stderr, "Error: Attempted to slice a non-list value.\n");
        exit(1);
    }
    if (!start_val || start_val->type != NUMBER_T) {
        fprintf(stderr, "Error: Slice start must be a number.\n");
        exit(1);
    }
    if (end_val && end_val->type != NUMBER_T && end_val->type != NIL_T) {
        fprintf(stderr, "Error: Slice end must be a number.\n");
        exit(1);
    }

    int start = (int)((NumberVal *)start_val)->value;
    int end = (!end_val || end_val->type == NIL_T)
        ? (int)((ListVal *)list_val)->size
        : (int)((NumberVal *)end_val)->value;
    return get_list_slice((ListVal *)list_val, start, end);
}

RuntimeVal *zox_rt_list_set(RuntimeVal *list_val, RuntimeVal *index_val, RuntimeVal *value) {
    if (!list_val || list_val->type != LIST_T) {
        fprintf(stderr, "Error: Attempted to index-assign a non-list value.\n");
        exit(1);
    }
    if (!index_val || index_val->type != NUMBER_T) {
        fprintf(stderr, "Error: List index must be a number.\n");
        exit(1);
    }

    ListVal *list = (ListVal *)list_val;
    int idx = (int)((NumberVal *)index_val)->value;
    if (idx < 0) idx = (int)list->size + idx;
    if (idx < 0 || idx >= (int)list->size) {
        fprintf(stderr, "Error: List index out of bounds.\n");
        exit(1);
    }

    retain(value);
    release(list->items[idx]);
    list->items[idx] = value;
    return value;
}

RuntimeVal *zox_rt_dict_get(RuntimeVal *dict_val, RuntimeVal *key_val) {
    if (!dict_val || dict_val->type != DICT_T) {
        fprintf(stderr, "Error: Attempted to key a non-dict value.\n");
        exit(1);
    }

    char *key = dict_key_to_string(key_val);
    if (!key) {
        fprintf(stderr, "Error: Dict key must be convertible to a string.\n");
        exit(1);
    }

    RuntimeVal *result = dict_get_val((DictVal *)dict_val, key);
    zox_free_buf(ZOX_BUF_DICT_KEY, key);
    return result ? result : (RuntimeVal *)MK_NIL();
}

RuntimeVal *zox_rt_dict_set(RuntimeVal *dict_val, RuntimeVal *key_val, RuntimeVal *value) {
    if (!dict_val || dict_val->type != DICT_T) {
        fprintf(stderr, "Error: Attempted to key-assign a non-dict value.\n");
        exit(1);
    }

    char *key = dict_key_to_string(key_val);
    if (!key) {
        fprintf(stderr, "Error: Dict key must be convertible to a string.\n");
        exit(1);
    }

    dict_set_val((DictVal *)dict_val, key, value);
    zox_free_buf(ZOX_BUF_DICT_KEY, key);
    return value;
}
