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
    if (callee && callee->type == TYPE_T) {
        TypeVal *type_def = (TypeVal *)callee;
        if (arg_count != type_def->field_count) {
            fprintf(stderr, "Error: Struct constructor argument count mismatch.\n");
            exit(1);
        }
        return (RuntimeVal *)MK_STRUCT_COPY_VALUES(type_def, args);
    }
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

RuntimeVal *zox_rt_unary_op(RuntimeVal *value, const char *op) {
    if (!value || value->type != NUMBER_T) {
        fprintf(stderr, "Error: Unary operator not applicable to non-number type.\n");
        exit(1);
    }

    if (strcmp(op, "-") == 0) {
        return (RuntimeVal *)MK_NUMBER(-((NumberVal *)value)->value);
    }
    if (strcmp(op, "+") == 0) {
        return (RuntimeVal *)MK_NUMBER(((NumberVal *)value)->value);
    }

    fprintf(stderr, "Error: Unsupported unary operator '%s'.\n", op ? op : "(null)");
    exit(1);
}

RuntimeVal *zox_rt_result_ok(RuntimeVal *value) {
    RuntimeVal *args[1] = { value };
    return builtin_ok(get_current_env(), args, 1);
}

RuntimeVal *zox_rt_result_err(RuntimeVal *value) {
    RuntimeVal *args[1] = { value };
    return builtin_err(get_current_env(), args, 1);
}

RuntimeVal *zox_rt_unwrap_ok(RuntimeVal *value) {
    RuntimeVal *ok_val;

    if (!value || value->type != DICT_T) {
        fprintf(stderr, "Error: Unwrap operator !? expects a result dictionary {ok: v} or {err: v}.\n");
        exit(1);
    }

    ok_val = dict_get_val((DictVal *)value, "ok");
    if (!ok_val) {
        fprintf(stderr, "Error: Unwrap operator !? expects a result dictionary with 'ok' or 'err' key.\n");
        exit(1);
    }

    retain(ok_val);
    return ok_val;
}

int zox_rt_unwrap_has_err(RuntimeVal *value) {
    if (!value || value->type != DICT_T) {
        fprintf(stderr, "Error: Unwrap operator !? expects a result dictionary {ok: v} or {err: v}.\n");
        exit(1);
    }

    return dict_get_val((DictVal *)value, "err") != NULL;
}

RuntimeVal *zox_rt_declare_type(const char *name, const char **fields, size_t field_count) {
    char **owned_fields;
    TypeVal *tv;
    size_t i;

    owned_fields = zox_alloc_buf(
        ZOX_BUF_TYPE_FIELDS, sizeof(char *) * field_count, "zox_rt_declare_type fields");
    for (i = 0; i < field_count; i++) {
        owned_fields[i] = zox_strdup_buf(ZOX_BUF_STRING, fields[i]);
    }

    tv = MK_TYPE(name, owned_fields, field_count);
    declare_var(get_current_env(), tv->name, (RuntimeVal *)tv);
    return (RuntimeVal *)tv;
}

RuntimeVal *zox_rt_member_get(RuntimeVal *object, const char *member) {
    StructVal *sv;
    size_t i;

    if (!object || object->type != STRUCT_T) {
        fprintf(stderr, "Error: Accessing member of non-struct value.\n");
        exit(1);
    }

    sv = (StructVal *)object;
    for (i = 0; i < sv->type_def->field_count; i++) {
        if (strcmp(sv->type_def->fields[i], member) == 0) {
            RuntimeVal *val = sv->values[i];
            retain(val);
            return val;
        }
    }

    fprintf(stderr, "Error: Struct 'type<%s>' has no field '%s'.\n", sv->type_def->name, member);
    exit(1);
}

RuntimeVal *zox_rt_member_set(RuntimeVal *object, const char *member, RuntimeVal *value) {
    StructVal *sv;
    size_t i;

    if (!object || object->type != STRUCT_T) {
        fprintf(stderr, "Error: Attempted to assign member of a non-struct value.\n");
        exit(1);
    }

    sv = (StructVal *)object;
    for (i = 0; i < sv->type_def->field_count; i++) {
        if (strcmp(sv->type_def->fields[i], member) == 0) {
            release(sv->values[i]);
            sv->values[i] = value;
            retain(value);
            return value;
        }
    }

    fprintf(stderr, "Error: Struct 'type<%s>' has no field '%s'.\n", sv->type_def->name, member);
    exit(1);
}

int zox_rt_match_cond(RuntimeVal *target, RuntimeVal *condition) {
    if (!condition) return 1;
    if (condition->type == BOOLEAN_T) {
        return ((BooleanVal *)condition)->value;
    }
    return compare_runtimeval(target, condition);
}

RuntimeVal *zox_rt_get_index(RuntimeVal *target_val, RuntimeVal *index_val) {
    if (!index_val || index_val->type != NUMBER_T) {
        fprintf(stderr, "Error: Index must be a number.\n");
        exit(1);
    }

    int idx = (int)((NumberVal *)index_val)->value;

    if (!target_val) {
        fprintf(stderr, "Error: Attempted to index a null value.\n");
        exit(1);
    }

    if (target_val->type == LIST_T) {
        ListVal *list = (ListVal *)target_val;
        if (idx < 0) idx = (int)list->size + idx;
        if (idx < 0 || idx >= (int)list->size) {
            fprintf(stderr, "Error: List index out of bounds.\n");
            exit(1);
        }
        RuntimeVal *result = list->items[idx];
        retain(result);
        return result;
    }

    if (target_val->type == STRING_T) {
        StringVal *str = (StringVal *)target_val;
        int len = (int)strlen(str->value);
        if (idx < 0) idx = len + idx;
        if (idx < 0 || idx >= len) {
            fprintf(stderr, "Error: String index out of bounds.\n");
            exit(1);
        }
        char single_char[2] = {str->value[idx], '\0'};
        return (RuntimeVal *)MK_STRING(single_char);
    }

    if (target_val->type == STRUCT_T) {
        StructVal *sv = (StructVal *)target_val;
        int field_count = (int)sv->type_def->field_count;
        if (idx < 0) idx = field_count + idx;
        if (idx < 0 || idx >= field_count) {
            fprintf(stderr, "Error: Struct index out of bounds.\n");
            exit(1);
        }
        RuntimeVal *result = sv->values[idx];
        retain(result);
        return result;
    }

    fprintf(stderr, "Error: Attempted to index a non-collection value.\n");
    exit(1);
}

RuntimeVal *zox_rt_get_slice(RuntimeVal *target_val, RuntimeVal *start_val, RuntimeVal *end_val) {
    if (!start_val || start_val->type != NUMBER_T) {
        fprintf(stderr, "Error: Slice start must be a number.\n");
        exit(1);
    }
    if (end_val && end_val->type != NUMBER_T && end_val->type != NIL_T) {
        fprintf(stderr, "Error: Slice end must be a number.\n");
        exit(1);
    }

    int start = (int)((NumberVal *)start_val)->value;

    if (!target_val) {
        fprintf(stderr, "Error: Attempted to slice a null value.\n");
        exit(1);
    }

    if (target_val->type == LIST_T) {
        int end = (!end_val || end_val->type == NIL_T)
            ? (int)((ListVal *)target_val)->size
            : (int)((NumberVal *)end_val)->value;
        return get_list_slice((ListVal *)target_val, start, end);
    }

    if (target_val->type == STRING_T) {
        StringVal *str = (StringVal *)target_val;
        int end = (!end_val || end_val->type == NIL_T)
            ? (int)strlen(str->value)
            : (int)((NumberVal *)end_val)->value;
        return get_string_slice(str, start, end);
    }

    fprintf(stderr, "Error: Attempted to slice a non-collection value.\n");
    exit(1);
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
