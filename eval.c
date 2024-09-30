#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include "malloc_safe.h"
#include "eval.h"
#include "values.h"
#include "env.h"
#include "hash.h"
#include "parser.h"
#include "global.h"
#include "native_modules.h"


#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

RuntimeVal *eval_program(Program *program, Environment *env) {
    RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();
    if (program->body_count == 0) {
        return lastEvaluated;
    }
    for (size_t i = 0; i < program->body_count; i++) {
        lastEvaluated = evaluate(program->body[i], env);
    }
    return lastEvaluated;
}

NumberVal *eval_numeric_binary_expr(NumberVal *lhs, NumberVal *rhs, const char *operator) {
    double result;
    int isComparison = 0;

    if (!strcmp(operator, "+")) {
        result = lhs->value + rhs->value;
    } else if (!strcmp(operator, "-")) {
        result = lhs->value - rhs->value;
    } else if (!strcmp(operator, "*")) {
        result = lhs->value * rhs->value;
    } else if (!strcmp(operator, "/")) {
        if (rhs->value == 0) {
            error("Error: Division by zero\n");
        }
        result = lhs->value / rhs->value;
    } else if (!strcmp(operator, "%")) {
        result = (int)lhs->value % (int)rhs->value;
    } else if (!strcmp(operator, "**")) {
        result = pow(lhs->value, rhs->value);
    } else if (!strcmp(operator, ">")) {
        result = (int)(lhs->value > rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, ">=")) {
        result = (int)(lhs->value >= rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, "<")) {
        result = (int)(lhs->value < rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, "<=")) {
        result = (int)(lhs->value <= rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, "==")) {
        result = (int)(lhs->value == rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, "!=")) {
        result = (int)(lhs->value != rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, "&&")) {
        result = (int)(lhs->value && rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, "||")) {
        result = (int)(lhs->value || rhs->value);
        isComparison = 1;
    } else if (!strcmp(operator, "&")) {
        result = (int)lhs->value & (int)rhs->value;
    } else if (!strcmp(operator, "|")) {
        result = (int)lhs->value | (int)rhs->value;
    } else if (!strcmp(operator, "^")) {
        result = (int)lhs->value ^ (int)rhs->value;
    } else if (!strcmp(operator, "<<")) {
        result = (int)lhs->value << (int)rhs->value;
    } else if (!strcmp(operator, ">>")) {
        result = (int)lhs->value >> (int)rhs->value;
    } else {
        char error_message[100];
        snprintf(error_message, sizeof(error_message), "Error: Unknown operator '%s'\n", operator);
        error(error_message);
    }

    if (isComparison) {
        return (NumberVal *)MK_BOOL(result);
    } else {
        return MK_NUMBER(result);
    }
}



ListVal *eval_list_binary_expr(ListVal *lhs, ListVal *rhs, const char *operator) {
    ListVal *new_list = NULL;
    if (!strcmp(operator, "*")) {
        size_t result_size = lhs->size * rhs->size;
        new_list = MK_LIST(result_size);
        size_t index = 0;
        for (size_t i = 0; i < lhs->size; i++) {
            for (size_t j = 0; j < rhs->size; j++) {
                ListVal *pair = MK_LIST(2);
                pair->items[0] = lhs->items[i];
                pair->items[1] = rhs->items[j];
                pair->size = 2;
                new_list->items[index] = (RuntimeVal *)pair;
                index++;
            }
        }
        new_list->size = result_size;
    } else if (!strcmp(operator, "+")) {
        new_list = MK_LIST(lhs->capacity + rhs->capacity);
        new_list->size = lhs->size + rhs->size;
        for (size_t i = 0; i < lhs->size; i++) {
            new_list->items[i] = lhs->items[i];
        }
        for (size_t i = 0; i < rhs->size; i++) {
            new_list->items[lhs->size + i] = rhs->items[i];
        }
    } else if (!strcmp(operator, "^")) {
        new_list = MK_LIST(lhs->capacity + rhs->capacity);
        new_list->size = 0;
        for (size_t i = 0; i < lhs->size; i++) {
            if (!contains(rhs, lhs->items[i])) {
                new_list->items[new_list->size++] = lhs->items[i];
            }
        }
        for (size_t i = 0; i < rhs->size; i++) {
            if (!contains(lhs, rhs->items[i])) {
                new_list->items[new_list->size++] = rhs->items[i];
            }
        }
    }
    return new_list;
}

RuntimeVal *eval_list_any_binary_expr(ListVal *lhs, RuntimeVal *rhs, const char *operator) {
    if (!strcmp(operator, "<<")) {
        if (lhs->size >= lhs->capacity) {
            lhs->capacity = lhs->capacity * 2 + 1;
            lhs->items = realloc_safe(lhs->items, sizeof(RuntimeVal*) * lhs->capacity, "eval_list_any_binary_expr realloc");
        }
        lhs->items[lhs->size] = rhs;
        lhs->size++;
        
        return (RuntimeVal *)lhs;
    }
    
    return NULL;
}

DictVal *eval_dict_binary_expr(DictVal *lhs, DictVal *rhs, const char *operator) {
    DictVal *new_dict = NULL;
    if (!strcmp(operator, "+")) {
        new_dict = MK_DICT(lhs->capacity + rhs->capacity);
        new_dict->size = lhs->size + rhs->size;
        for (size_t i = 0; i < lhs->capacity; i++) {
            Entry *entry = lhs->entries[i];
            if (entry != NULL) {
                dict_set_val(
                    new_dict,
                    entry->key,
                    entry->value  
                );
            }
        }
        for (size_t i = 0; i < rhs->capacity; i++) {
            Entry *entry = rhs->entries[i];
            if (entry != NULL) {
                dict_set_val(
                    new_dict,
                    entry->key,
                    entry->value  
                );
            }
        }
    } 
    return new_dict;
}

RuntimeVal *eval_var_expr(VarDeclaration *var, Environment *env) {
    RuntimeVal *value = evaluate(&(var->value->stmt), env);
    declare_var(env, var->varname, value);
    return value;
}

RuntimeVal *eval_string_literal(StringLiteral *str_literal) {
    return (RuntimeVal *)MK_STRING(str_literal->value);
}

RuntimeVal *eval_assign_var_expr(AssignVar *var, Environment *env) {
    RuntimeVal *value = evaluate(&(var->value->stmt), env);
    lookup_var(env, var->varname);
    assign_var(env, var->varname, value);
    return value;
}

RuntimeVal *eval_assign_list_var_expr(AssignListVar *var, Environment *env) {
    RuntimeVal *value = evaluate(&(var->value->stmt), env);
    NumberVal *index = (NumberVal *)evaluate(&(var->index->stmt), env);
    ListVal *list = (ListVal *)lookup_var(env, var->varname);
    list->items[(int) index->value] = value;
    return value;
}

RuntimeVal *eval_assign_dict_var_expr(AssignDictVar *var, Environment *env) {
    RuntimeVal *value = evaluate(&(var->value->stmt), env);  
    StringVal *key = (StringVal *)evaluate(&(var->key->stmt), env);
    DictVal *dict = (DictVal *)lookup_var(env, var->varname);
    dict_set_val(
        dict,
        key->value,
        value  
    );
    return value;
}

static RuntimeVal *eval_string_binary_expr(StringVal *lhs, StringVal *rhs, const char *operator) {
    if (!strcmp(operator, "+")) {
        size_t new_size = strlen(lhs->value) + strlen(rhs->value);
        char *new_value = malloc_safe(new_size + 1, "eval_string_binary_expr new_value");
        strcpy(new_value, lhs->value);
        strcat(new_value, rhs->value);
        RuntimeVal *result = (RuntimeVal *)MK_STRING(new_value);    
        free_safe(new_value);
        return result;
    } else if (!strcmp(operator, "-")) {
        int len_a = strlen(lhs->value); 
        int len_b = strlen(rhs->value);
        int i, j;
        char* result = (char*)malloc_safe(len_a + 1, "eval_string_binary_expr result");
        int result_index = 0;  
        for (i = 0; i < len_a; ) {
            for (j = 0; j < len_b && lhs->value[i + j] == rhs->value[j]; j++);
            if (j == len_b) {
                i += len_b;
            } else {
                result[result_index++] = lhs->value[i++];
            }
        }        
        result[result_index] = '\0';
        RuntimeVal *r = (RuntimeVal *)MK_STRING(result);
        free_safe(result);
        return r;
    }
    error("Unsupported operator for string binary expression");
    return NULL;
}

RuntimeVal *eval_string_repeat(StringVal *str, NumberVal *num) {
    int repeat_count = (int)num->value;
    if (repeat_count < 0) {
        error("Cannot repeat string a negative number of times");
    }
    
    char *new_value = malloc_safe(strlen(str->value) * repeat_count + 1, "eval_string_repeat new_value");
    new_value[0] = '\0';

    for (int i = 0; i < repeat_count; i++) {
        strcat(new_value, str->value);
    }
    
    RuntimeVal *result = (RuntimeVal *)MK_STRING(new_value);
    free_safe(new_value);
    return result;
}

RuntimeVal *eval_binary_expr(BinaryExpr *binop, Environment *env) {
    RuntimeVal *lhs = evaluate(&(binop->left->stmt), env);
    RuntimeVal *rhs = evaluate(&(binop->right->stmt), env);

    if ((lhs->type == NUMBER_T || lhs->type == BOOLEAN_T) &&
        (rhs->type == NUMBER_T || rhs->type == BOOLEAN_T)) {
        NumberVal *lhs_num, *rhs_num;

        if (lhs->type == NUMBER_T) {
            lhs_num = (NumberVal *)lhs;
        } else {
            lhs_num = MK_NUMBER(((BooleanVal *)lhs)->value ? 1 : 0);
        }
        
        if (rhs->type == NUMBER_T) {
            rhs_num = (NumberVal *)rhs;
        } else {
            rhs_num = MK_NUMBER(((BooleanVal *)rhs)->value ? 1 : 0);
        }
        
        RuntimeVal *result = (RuntimeVal *)eval_numeric_binary_expr(lhs_num, rhs_num, binop->operator);
        
        if (lhs->type == BOOLEAN_T) {
            free_safe(lhs_num);
        }
        if (rhs->type == BOOLEAN_T) {
            free_safe(rhs_num);
        }   
        return result;
    }
    if (lhs->type == STRING_T && rhs->type == STRING_T) {
        return eval_string_binary_expr((StringVal *)lhs, (StringVal *)rhs, binop->operator);
    }
    if (lhs->type == STRING_T && rhs->type == NUMBER_T) {
        if (!strcmp(binop->operator, "*")) {
            StringVal *str = (StringVal *)lhs;
            NumberVal *num = (NumberVal *)rhs;
            return eval_string_repeat((StringVal *)lhs, (NumberVal *)rhs);
        }
    }
    if (lhs->type == LIST_T && rhs->type == LIST_T) {
        return (RuntimeVal *)eval_list_binary_expr(
            (ListVal *)lhs, (ListVal *)rhs, binop->operator
        );
    }
    if (lhs->type == LIST_T && rhs->type != LIST_T) {
        return (RuntimeVal *)eval_list_any_binary_expr(
            (ListVal *)lhs, (RuntimeVal *)rhs, binop->operator
        );
    }
    if (lhs->type == DICT_T && rhs->type == DICT_T) {
        return (RuntimeVal *)eval_dict_binary_expr( 
            (DictVal *)lhs, (DictVal *)rhs, binop->operator
        );
    }
    if (lhs->type == TABLE_T && rhs->type == DICT_T) {
        TableVal *table = (TableVal *)lhs;
        DictVal *dict = (DictVal *)rhs;
        if (!strcmp(binop->operator, "+")) {
            if (dict->size != table->column_count) {
                error("Error: Dictionary size does not match table column count.\n");
            }
            
            if (table->row_count >= table->capacity) {
                table->capacity = table->capacity == 0 ? 1 : table->capacity * 2;
                table->rows = realloc_safe(table->rows, sizeof(DictVal *) * table->capacity, "eval_binary_expr table rows");
            }
            
            table->rows[table->row_count++] = dict;
        }
        return lhs;
    }
    if (lhs->type == TABLE_T && rhs->type == LIST_T) {
        TableVal *table = (TableVal *)lhs;
        ListVal *list = (ListVal *)rhs;
        if (!strcmp(binop->operator, "+")) {
            for (size_t i = 0; i < list->size; i++) {
                DictVal *dict;
                if (list->items[i]->type == DICT_T) {
                    dict = (DictVal *)list->items[i];
                } else if (list->items[i]->type == LIST_T) {
                    ListVal *inner_list = (ListVal *)list->items[i];
                    if (inner_list->size != table->column_count) {
                        error("Error: Inner list size does not match table column count.\n");
                    }
                    dict = MK_DICT(table->column_count);
                    for (size_t j = 0; j < table->column_count; j++) {
                        dict_set_val(dict, table->columns[j], inner_list->items[j]);
                    }
                } else {
                    error("Error: All items in the list must be dictionaries or lists.\n");
                }
                
                if (dict->size != table->column_count) {
                    error("Error: Dictionary size does not match table column count.\n");
                }
                
                if (table->row_count >= table->capacity) {
                    table->capacity = table->capacity == 0 ? 1 : table->capacity * 2;
                    table->rows = realloc_safe(table->rows, sizeof(DictVal *) * table->capacity, "eval_binary_expr table rows");
                }
                
                table->rows[table->row_count++] = dict;
            }
        }
        return (RuntimeVal *)table;
    }
    if (lhs->type != rhs->type) {
        return (RuntimeVal *)MK_BOOL(0);
    }
    char error_message[100];
    snprintf(error_message, sizeof(error_message), "Unsupported types (%s, %s) for operator %s\n",
        type_to_string(lhs->type),
        type_to_string(rhs->type),
        binop->operator);
    error(error_message);
}


RuntimeVal *eval_identifier_expr(Identifier *ident, Environment *env) {
    return lookup_var(env, ident->symbol);  
}

short int is_while_finished(WhileExpr *while_expr, Environment *env) {
    RuntimeVal *condition_val = evaluate(&(while_expr->condition->stmt), env);
    if (condition_val->type != BOOLEAN_T) {
        error("Condition of '#' must be a boolean.\n");   
    }
    return ((BooleanVal *)condition_val)->value;
}

RuntimeVal *eval_while_expr(WhileExpr *while_expr, Environment *env) {
    Environment *while_env = create_environment(env, "while_env");
    RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();
    while (is_while_finished(while_expr, while_env)) {
        for (size_t i = 0; i < while_expr->body_count; i++) {
            lastEvaluated = evaluate(while_expr->body[i], while_env);
        }
    }
    return lastEvaluated;
}

RuntimeVal *eval_if_expr(IfExpr *if_expr, Environment *env) {
    Environment *if_env = create_environment(env, "if_env");
    RuntimeVal *condition_val = evaluate(&(if_expr->condition->stmt), if_env);
    if (condition_val->type != BOOLEAN_T) {
        error("Condition of '?' must be a boolean.\n");
    }
    if (((BooleanVal *)condition_val)->value) {
        RuntimeVal *lastEvaluated = NULL;
        for (size_t i = 0; i < if_expr->body_count; i++) {
            lastEvaluated = evaluate(if_expr->body[i], if_env);
        }
        return lastEvaluated;
    }
    if (if_expr->else_if != NULL) {
        Environment *else_if_env = create_environment(env, "else_if_env");
        RuntimeVal *else_if_r = evaluate((Stmt *)if_expr->else_if, else_if_env);
        if (else_if_r != NULL) {
            return else_if_r;
        }
    }
    if (if_expr->else_body != NULL) {
        Environment *else_env = create_environment(env, "else_env");
        RuntimeVal *lastEvaluated = NULL;
        for (size_t i = 0; i < if_expr->else_body_count; i++) {
            lastEvaluated = evaluate(if_expr->else_body[i], else_env);
        }
        return lastEvaluated;
    }
    return NULL;
}

RuntimeVal *eval_for_expr(ForExpr *for_expr, Environment *env) {
    Environment *for_env = create_environment(env, "for_env");
    RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();

    evaluate((Stmt *)for_expr->initialization, for_env);

    while (1) {
        RuntimeVal *condition_val = evaluate(&(for_expr->condition->stmt), for_env);
        if (condition_val->type != BOOLEAN_T) {
            error("Condition of '@' must be a boolean.\n");
        }
        if (!((BooleanVal *)condition_val)->value) {
            break;
        }
        Environment *for_env_loop = create_environment(for_env, "for_env_loop");
        for (size_t i = 0; i < for_expr->body_count; i++) {
            lastEvaluated = evaluate(for_expr->body[i], for_env_loop);
        }
        free_environment(for_env_loop);
        evaluate((Stmt *)for_expr->increment, for_env);
    }
    free_environment(for_env);
    return lastEvaluated;
}

RuntimeVal *eval_func_def(FuncDef *func_def, Environment *env) {
    FunctionVal *func_val = MK_FUNCTION(func_def->params, func_def->param_count, func_def->body, func_def->body_count, env, NULL);
    declare_var(env, func_def->name, (RuntimeVal *)func_val);
    return (RuntimeVal *)func_val;
}

RuntimeVal *eval_call_expr(CallExpr *call_expr, Environment *env) {
    RuntimeVal *callee = evaluate(&(call_expr->callee->stmt), env);
    if (callee->type != FUNCTION_T) {
        error("Attempted to call a non-function value.\n");
    }
    FunctionVal *func = (FunctionVal *)callee;
    if (call_expr->arg_count != func->param_count) {
        char error_message[100];
        snprintf(error_message, sizeof(error_message), "Function expected %ld arguments but got %ld.\n", func->param_count, call_expr->arg_count);
        error(error_message);
    }
    if (func->builtin_func != NULL) {
        RuntimeVal **args = malloc_safe(sizeof(RuntimeVal *) * call_expr->arg_count, "eval_call_expr args");
        for (size_t i = 0; i < call_expr->arg_count; i++) {
            args[i] = evaluate(&(call_expr->arguments[i]->stmt), env);
        }
        RuntimeVal *result = func->builtin_func(env, args, call_expr->arg_count);
        free_safe(args);
        return result;
    }
    Environment *func_env = create_environment(func->env, "func_env");
    for (size_t i = 0; i < func->param_count; i++) {
        RuntimeVal *arg_val = evaluate(&(call_expr->arguments[i]->stmt), env);
        declare_var(func_env, func->params[i], arg_val);
    }
    RuntimeVal *lastEvaluated = (RuntimeVal *)MK_NIL();
    for (size_t i = 0; i < func->body_count; i++) {
        lastEvaluated = evaluate(func->body[i], func_env);
    }
    free_environment(func_env);
    return lastEvaluated;
}

void list_append_val(ListVal *list, RuntimeVal *item) {
    list->items[list->size++] = item;
}

RuntimeVal *eval_list_literal(ListLiteral *list_lit, Environment *env) {
    ListVal *list = MK_LIST(list_lit->element_count * 2);
    for (size_t i = 0; i < list_lit->element_count; i++) {
        list_append_val(list, evaluate(&(list_lit->elements[i]->stmt), env));
    }
    return (RuntimeVal *)list;
}

void resize_dict(DictVal *dict) {
    size_t new_capacity = dict->capacity * 2;
    Entry **new_entries = (Entry **)malloc_safe(new_capacity * sizeof(Entry *), "resize_dict new_entries");
    for (size_t i = 0; i < new_capacity; i++) {
        new_entries[i] = NULL;
    }
    for (size_t i = 0; i < dict->capacity; i++) {
        Entry *entry = dict->entries[i];
        while (entry != NULL) {
            size_t new_slot = hash(entry->key, new_capacity);
            Entry *next_entry = entry->next;
            entry->next = new_entries[new_slot];
            new_entries[new_slot] = entry;
            entry = next_entry;
        }
    }
    free(dict->entries);
    dict->entries = new_entries;
    dict->capacity = new_capacity;
}

void dict_set_val(DictVal *dict, const char *key, RuntimeVal *value) {
    size_t slot = hash(key, dict->capacity);

    Entry *entry = dict->entries[slot];
    if (entry == NULL) {
        dict->entries[slot] = MK_ENTRY(key, value);
        dict->size++;
    } else {
        Entry *prev;
        while (entry != NULL) {
            if (strcmp(entry->key, key) == 0) {
                entry->value = value;
                return;
            }
            prev = entry;
            entry = prev->next;
        }
        prev->next = MK_ENTRY(key, value);
        dict->size++;
    }

    // if ((double)dict->capacity / dict->size >= 0.75) {
    //     resize_dict(dict);
    // }
}

RuntimeVal *eval_table_literal(TableLiteral *table_lit, Environment *env) {
    return (RuntimeVal *)MK_TABLE(table_lit->columns, table_lit->column_count);
}

char* runtime_value_to_string(RuntimeVal *val) {
    switch (val->type) {
        case NIL_T:
            return "nil";
        case BOOLEAN_T: {
            BooleanVal *bool_val = (BooleanVal *)val;
            if (bool_val->value) {
                return "true";
            }
            return "false";
        }
        case NUMBER_T: {
            NumberVal *num_val = (NumberVal *)val;
            // Assume 32 bytes are enough to hold the string representation of the number
            char *result = malloc_safe(32, "runtime_value_to_string NUMBER_T");
            sprintf(result, "%f", num_val->value);
            return result;
        }
        case STRING_T: {
            StringVal *str_val = (StringVal *)val;
            return str_val->value;
        }
        default:
            // Handle unexpected type
            return NULL;
    }
}

RuntimeVal *eval_dict_literal(DictLiteral *dict_lit, Environment *env) {
    DictVal *dict = MK_DICT(dict_lit->element_count * 2);
    for (size_t i = 0; i < dict_lit->element_count; i++) {
        RuntimeVal *key = evaluate(&(dict_lit->keys[i]->stmt), env);
        RuntimeVal *value = evaluate(&(dict_lit->values[i]->stmt), env);
        dict_set_val(
            dict,
            runtime_value_to_string(key),
            value  
        );
    }
    return (RuntimeVal *)dict;
}

RuntimeVal *get_list_slice(ListVal *list, int start, int end) {
    if (start < 0) start = list->size + start;
    if (end < 0) end = list->size + end;
    
    start = (start < 0) ? 0 : (start > list->size) ? list->size : start;
    end = (end < 0) ? 0 : (end > list->size) ? list->size : end;
    
    if (start >= end) return (RuntimeVal *)MK_LIST(0);
    
    ListVal *slice = MK_LIST(end - start);
    for (int i = start; i < end; i++) {
        list_append_val(slice, list->items[i]);
    }
    return (RuntimeVal *)slice;
}

RuntimeVal *get_table_slice(TableVal *table, int start, int end) {
    if (start < 0) start = table->row_count + start;
    if (end < 0) end = table->row_count + end;
    
    start = (start < 0) ? 0 : (start > table->row_count) ? table->row_count : start;
    end = (end < 0) ? 0 : (end > table->row_count) ? table->row_count : end;
    
    if (start >= end) return (RuntimeVal *)MK_TABLE(table->columns, table->column_count);
    
    TableVal *slice = MK_TABLE(table->columns, table->column_count);
    for (int i = start; i < end; i++) {
        if (slice->row_count >= slice->capacity) {
            slice->capacity = slice->capacity == 0 ? 1 : slice->capacity * 2;
            slice->rows = realloc_safe(slice->rows, sizeof(DictVal *) * slice->capacity, "get_table_slice rows");
        }
        slice->rows[slice->row_count++] = table->rows[i];
    }
    return (RuntimeVal *)slice;
}

RuntimeVal *get_string_slice(StringVal *str, int start, int end) {
    size_t size = strlen(str->value);
    if (start < 0) start = size + start;
    if (start < 0 || start >= size) {
        error("String index out of bounds.\n");
    }
    if (end < 0) end = size + end;
    if (end < 0 || end > size) {
        error("String index out of bounds.\n");
    }
    if (start >= end) return (RuntimeVal *)MK_STRING("");
    
    char *slice = malloc_safe(end - start + 1, "get_string_slice slice");
    strncpy(slice, str->value + start, end - start);
    slice[end - start] = '\0';
    return (RuntimeVal *)MK_STRING(slice);
}

RuntimeVal *eval_list_index(ListIndex *list_index, Environment *env) {
    RuntimeVal *list_val = evaluate(&(list_index->list->stmt), env);
    if (list_val->type != LIST_T && list_val->type != TABLE_T && list_val->type != STRING_T) {
        error("Attempted to index a non-list value.\n");
    }
    
    RuntimeVal *start_val = evaluate(&(list_index->start->stmt), env);
    if (start_val->type != NUMBER_T) {
        error("Start index must be a number.\n");
    }
    int start = (int)((NumberVal *)start_val)->value;
    
    if (list_val->type == STRING_T) {
        StringVal *str = (StringVal *)list_val;
        if (!list_index->is_slice) {
            if (start < 0) start = strlen(str->value) + start;
            if (start < 0 || start >= strlen(str->value)) {
                error("String index out of bounds.\n");
            }
            char single_char[2] = {str->value[start], '\0'};
            return (RuntimeVal *)MK_STRING(single_char);
        } else {
            int end = strlen(str->value);
            if (list_index->end != NULL) {
                RuntimeVal *end_val = evaluate(&(list_index->end->stmt), env);
                if (end_val->type != NUMBER_T) {
                    error("String end index must be a number.\n");
                }
                end = (int)((NumberVal *)end_val)->value;
            }
            return get_string_slice(str, start, end);
        }
    } else if (list_val->type == LIST_T) {
        ListVal *list = (ListVal *)list_val;
        if (!list_index->is_slice) {
            if (start < 0) start = list->size + start;
            if (start < 0 || start >= list->size) {
                error("List index out of bounds.\n");
            }
            if (start < 0) start = list->size + start;
            return list->items[start];
        } else {
            int end = list->size;
            if (list_index->end != NULL) {
                RuntimeVal *end_val = evaluate(&(list_index->end->stmt), env);
                if (end_val->type != NUMBER_T) {
                    error("List end index must be a number.\n");
                }
                end = (int)((NumberVal *)end_val)->value;
            }
            return get_list_slice(list, start, end);
        }
    } else {
        TableVal *table = (TableVal *)list_val;
        if (!list_index->is_slice) {
            if (start < -table->row_count || start >= table->row_count) {
                error("List index out of bounds.\n");
            }
            if (start < 0) start = table->row_count + start;
            return (RuntimeVal *)table->rows[start];
        } else {
            int end = table->row_count;
            if (list_index->end != NULL) {
                RuntimeVal *end_val = evaluate(&(list_index->end->stmt), env);
                if (end_val->type != NUMBER_T) {
                    error("List end index must be a number.\n");
                }
                end = (int)((NumberVal *)end_val)->value;
            }
            return get_table_slice(table, start, end);
        }
    }
}

RuntimeVal *eval_dict_key(DictKey *dict_key, Environment *env) {
    RuntimeVal *dict_val = evaluate(&(dict_key->dict->stmt), env);
    if (dict_val->type != DICT_T) {
        error("Attempted to key a non-dict value.\n");
    }
    DictVal *dict = (DictVal *)dict_val;
    RuntimeVal *key_val = evaluate(&(dict_key->key->stmt), env);
    char *key = runtime_value_to_string(key_val);
    if (key == NULL) {
        error("Dict key must be convertible to a hashable string.\n");
    }
    size_t slot = hash(key, dict->capacity);
    for (Entry *entry = dict->entries[slot]; entry != NULL; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
    }
    return NULL;
}

char *find_module_path(const char *module_name) {
    for (int i = 0; native_modules[i].name != NULL; i++) {
        if (strcmp(native_modules[i].name, module_name) == 0) {
            return strdup("native");
        }
    }

    #ifdef _WIN32
        char *paths[] = {".", ".\\packages", "C:\\Program Files\\Zox\\packages"};
    #else
        char *paths[] = {".", "./packages", "/usr/local/lib/zox/packages"};
    #endif
    char full_path[512];
    char module_path[256];
    
    // Substituir '.' por separador de diretório
    strncpy(module_path, module_name, sizeof(module_path));
    char *p = module_path;
    while (*p) {
        if (*p == '.') *p = PATH_SEPARATOR[0];
        p++;
    }

    for (int i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        snprintf(full_path, sizeof(full_path), "%s%s%s.zo", paths[i], PATH_SEPARATOR, module_path);
        if (access(full_path, F_OK) != -1) {
            return strdup(full_path);
        }
    }
    return NULL;
}


RuntimeVal *eval_import_stmt(ImportStmt *import_stmt, Environment *env) {
    char *module_path = find_module_path(import_stmt->module_name);
    if (!module_path) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Module '%s' not found", import_stmt->module_name);
        error(error_msg);
    }
    
    if (strcmp(module_path, "native") == 0) {
        for (int i = 0; native_modules[i].name != NULL; i++) {
            if (strcmp(native_modules[i].name, import_stmt->module_name) == 0) {
                Environment *module_env = create_environment(env, import_stmt->module_name);
                native_modules[i].init_func(module_env);
                
                if (import_stmt->import_count > 0) {
                    for (size_t j = 0; j < import_stmt->import_count; j++) {
                        ImportItem *item = import_stmt->imports[j];
                        RuntimeVal *imported_value = lookup_var(module_env, item->name);
                        if (imported_value == NULL) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg), "Cannot find '%s' in module '%s'", item->name, import_stmt->module_name);
                            error(error_msg);
                        }
                        declare_var(env, item->alias ? item->alias : item->name, imported_value);
                    }
                } else {
                    declare_var(env, import_stmt->module_name, (RuntimeVal *)module_env);
                }
                free_safe(module_path);
                return (RuntimeVal *)MK_NIL();
            }
        }
    }

    char *module_code = read_file(module_path);
    Environment *module_env = create_environment(env, import_stmt->module_name);

    size_t token_count;
    Token *tokens = tokenize(module_code, &token_count);
    Parser *parser = create_parser(tokens, token_count);
    Program *program = produce_ast(parser, module_code);
    eval_program(program, module_env);
    
    if (import_stmt->import_count > 0) {
        for (size_t i = 0; i < import_stmt->import_count; i++) {
            ImportItem *item = import_stmt->imports[i];
            char *name = item->name;
            RuntimeVal *imported_value = lookup_var(module_env, name);
            
            if (imported_value == NULL) {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "Cannot find '%s' in module '%s'", name, import_stmt->module_name);
                error(error_msg);
            }
            
            declare_var(env, item->alias ? item->alias : name, imported_value);
        }
    } else {
        declare_var(env, import_stmt->module_name, (RuntimeVal *)module_env);
    }
    
    free_safe(module_code);
    free_tokens(tokens, token_count);
    free_safe(parser);
    free_program(program);
    
    return (RuntimeVal *)MK_NIL();
}

RuntimeVal *evaluate(Stmt *astNode, Environment *env) {
    switch (astNode->kind) {
        case ProgramAst: {
            return eval_program((Program *)astNode, env);
        }
        case BooleanLiteralAst: {
            return (RuntimeVal *)MK_BOOL(((BooleanLiteral *)astNode)->value);
        }
        case NilAst: {
            printf("NIL");
            return (RuntimeVal *)MK_NIL();
        }
        case NumericLiteralAst: {
            return (RuntimeVal *)MK_NUMBER(((NumericLiteral *)astNode)->value);
        }
        case IdentifierAst: {
            return eval_identifier_expr((Identifier *)astNode, env);
        }
        case BinaryExprAst: {
            return eval_binary_expr((BinaryExpr *)astNode, env);
        }
        case VarDeclarationAst: {
            return eval_var_expr((VarDeclaration *)astNode, env);
        }
        case AssignVarAst: {
            return eval_assign_var_expr((AssignVar *)astNode, env);
        }
        case IfAst: {
            return eval_if_expr((IfExpr *)astNode, env);
        }
        case WhileAst: {
            return eval_while_expr((WhileExpr *)astNode, env);
        }
        case ForAst: {
            return eval_for_expr((ForExpr *)astNode, env);
        }
        case StringLiteralAst: {
            return eval_string_literal((StringLiteral *)astNode);
        }
        case FuncDefAst: {
            return eval_func_def((FuncDef *)astNode, env);
        }
        case CallExprAst: {
            return eval_call_expr((CallExpr *)astNode, env);
        }
        case ListLiteralAst: {
            return eval_list_literal((ListLiteral *)astNode, env);
        }
        case DictLiteralAst: {
            return eval_dict_literal((DictLiteral *)astNode, env);
        }
        case ListIndexAst: {
           return eval_list_index((ListIndex *)astNode, env);
        }
        case DictKeyAst: {
           return eval_dict_key((DictKey *)astNode, env);
        }
        case AssignListVarAst: {
            return eval_assign_list_var_expr((AssignListVar *)astNode, env);
        }
        case AssignDictVarAst: {
            return eval_assign_dict_var_expr((AssignDictVar *)astNode, env);
        }
        case TableLiteralAst: {
            return eval_table_literal((TableLiteral *)astNode, env);
        }
        case ImportAst: {
            return eval_import_stmt((ImportStmt *)astNode, env);
        }
        default: {
            error("This AST Node has not yet been setup for interpretation.\n");
        }
    }
}
