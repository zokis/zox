// builtins.c
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "builtins.h"
#include "eval.h"
#include "malloc_safe.h"
#include "hash.h"
#include "values.h"
#include "global.h"


RuntimeVal *builtin_find(Environment *env, RuntimeVal **args, size_t arg_count) {
    if (arg_count != 2) {
        error("Function 'find' expects exactly two arguments.");
    }
    if (args[0]->type != STRING_T && args[0]->type != LIST_T) {
        error("The first argument for 'find' must be a string or list.");
    }
    if (args[1]->type != STRING_T && args[1]->type != NUMBER_T && args[1]->type != BOOLEAN_T) {
        error("The second argument for 'find' must be a string, number or boolean.");
    } 
    if (args[0]->type == STRING_T) {
        StringVal *str = (StringVal *)args[0];
        StringVal *value = (StringVal *)args[1];
        char *str_value = str->value;
        char *value_value = value->value;
        char *pos = strstr(str_value, value_value);
        if (pos != NULL) {
            return (RuntimeVal *)MK_NUMBER((double)(pos - str_value));      
        }
    }
    if (args[0]->type == LIST_T) {
        ListVal *list = (ListVal *)args[0];
        RuntimeVal *value = args[1];
        if (contains(list, value)) {
            return (RuntimeVal *)MK_NUMBER((double)1);
        }
    }   
    return (RuntimeVal *)MK_NUMBER((double)-1);
}

RuntimeVal *builtin_keys(Environment *env, RuntimeVal **args, size_t arg_count) {
    if (arg_count != 1) {
        error("Function 'keys' expects exactly one argument.");
    }
    if (args[0]->type != DICT_T) {
        error("Argument to 'keys' must be a dictionary.");
    }

    DictVal *dict = (DictVal *)args[0];
    ListVal *keys_list = MK_LIST(dict->size);

    for (size_t i = 0; i < dict->capacity; i++) {
        Entry *entry = dict->entries[i];
        while (entry != NULL) {
            keys_list->items[keys_list->size++] = (RuntimeVal *)MK_STRING(entry->key);
            entry = entry->next;
        }
    }

    return (RuntimeVal *)keys_list;
}

RuntimeVal *builtin_values(Environment *env, RuntimeVal **args, size_t arg_count) {
    if (arg_count != 1) {
        error("The 'values' function expects exactly one argument.");
    }
    if (args[0]->type != DICT_T) {
        error("The argument for 'values' must be a dictionary.");
    }

    DictVal *dict = (DictVal *)args[0];
    ListVal *values_list = MK_LIST(dict->size);

    for (size_t i = 0; i < dict->capacity; i++) {
        Entry *entry = dict->entries[i];
        while (entry != NULL) {
            values_list->items[values_list->size++] = entry->value;
            entry = entry->next;
        }
    }

    return (RuntimeVal *)values_list;
}

RuntimeVal *builtin_len(Environment *env, RuntimeVal **args, size_t arg_count) {
    if (arg_count != 1) {
        error("Function 'len' expects exactly one argument.");
    }
    if (args[0]->type == LIST_T) {
        ListVal *list = (ListVal *)args[0];
        return (RuntimeVal *)MK_NUMBER((double)list->size);
    } else if (args[0]->type == STRING_T) {
        StringVal *str = (StringVal *)args[0];
        return (RuntimeVal *)MK_NUMBER((double)strlen(str->value));
    } else if (args[0]->type == DICT_T) {
        DictVal *dict = (DictVal *)args[0];
        return (RuntimeVal *)MK_NUMBER((double)dict->size);
    } else if (args[0]->type == TABLE_T) {
        TableVal *table = (TableVal *)args[0];
        return (RuntimeVal *)MK_NUMBER((double)table->row_count);
    } else {
        error("Argument to 'len' must be a table, list, string or dictionary.");
    }
}

void _builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count, bool as_string) {
    if (arg_count != 1) {
        error("Function 'print' expects exactly one argument.");
    }
    RuntimeVal *val = args[0];

    switch (val->type) {
        case NIL_T:
            printf("nil");
            break;
        case BOOLEAN_T: {
            BooleanVal *bool_val = (BooleanVal *)val;
            printf("%s", bool_val->value ? "true" : "false");
            break;
        }
        case NUMBER_T: {
            NumberVal *num_val = (NumberVal *)val;
            printf("%f", num_val->value);
            break;
        }
        case STRING_T: {
            StringVal *str_val = (StringVal *)val;
            if (as_string) {
                printf("\"%s\"", str_val->value);
            } else {
                printf("%s", str_val->value);
            }
            break;
        }
        case LIST_T: {
            ListVal *list_val = (ListVal *)val;
            printf("{");
            for (size_t i = 0; i < list_val->size; i++) {
                if (i > 0) {
                    printf(", ");
                }
                RuntimeVal *item = list_val->items[i];
                RuntimeVal *item_args[] = {item};
                _builtin_print_value(env, item_args, 1, 1);
            }
            printf("}");
            break;
        }
        case TABLE_T: {
            TableVal *table_val = (TableVal *)val;
            printf("|>");
            for (size_t i = 0; i < table_val->column_count; i++) {
                if (i > 0) {
                    printf(";");
                }
                printf("%s", table_val->columns[i]);
            }
            printf("<|");
            printf("{%zu}", table_val->row_count);
            break;
        }
        case DICT_T: {
            DictVal *dict_val = (DictVal *)val;
            printf("[");
            short int first = 1;
            for (size_t i = 0; i < dict_val->capacity; i++) {
                Entry *entry = dict_val->entries[i];
                while (entry != NULL) {
                    if (!first) {
                        printf("; ");
                    }
                    printf("\"%s\" -> ", entry->key);
                    RuntimeVal *entry_args[] = {entry->value};
                    _builtin_print_value(env, entry_args, 1, 1);
                    entry = entry->next;
                    first = 0;
                }
            }
            printf("]");
            break;
        }
        case FUNCTION_T: {
            FunctionVal *func_val = (FunctionVal *)val;
            printf("<function>");
            break;
        }
        default:
            printf("Unknown value type\n");
    }
}

RuntimeVal *builtin_println_value(Environment *env, RuntimeVal **args, size_t arg_count) {
    _builtin_print_value(env, args, 1, 0);
    printf("\n");
    return (RuntimeVal *)MK_NIL();
}


RuntimeVal *builtin_random(Environment *env, RuntimeVal **args, size_t arg_count) {
    static int initialized = 0;
    if (!initialized) {
        srand(time(NULL));
        initialized = 1;
    }
    double random_value = (double)rand() / RAND_MAX;
    return (RuntimeVal *)MK_NUMBER(random_value);
}

RuntimeVal *builtin_random_int(Environment *env, RuntimeVal **args, size_t arg_count) {
    if (arg_count != 2) {
        fprintf(stderr, "Error: random_int expects 2 arguments, but received %zu\n", arg_count);
        return (RuntimeVal *)MK_NIL();
    }

    RuntimeVal *min_val = args[0];
    RuntimeVal *max_val = args[1];

    if (min_val->type != NUMBER_T || max_val->type != NUMBER_T) {
        fprintf(stderr, "Error: random_int expects two numbers as arguments\n");
        return (RuntimeVal *)MK_NIL();
    }

    int min = (int)((NumberVal *)min_val)->value;
    int max = (int)((NumberVal *)max_val)->value;

    if (min > max) {
        fprintf(stderr, "Error: the first argument must be less than or equal to the second\n");
        return (RuntimeVal *)MK_NIL();
    }

    static int initialized = 0;
    if (!initialized) {
        srand(time(NULL));
        initialized = 1;
    }

    int random_int = min + rand() % (max - min + 1);
    return (RuntimeVal *)MK_NUMBER((double)random_int);
}

RuntimeVal *builtin_print_value(Environment *env, RuntimeVal **args, size_t arg_count) {
    _builtin_print_value(env, args, 1, 0);
    return (RuntimeVal *)MK_NIL();
}

void register_builtins(Environment *env) {
    char *keys_params[] = {"dict"};
    size_t keys_param_count = 1;
    declare_var(env, "keys", (RuntimeVal *)MK_FUNCTION(keys_params, keys_param_count, NULL, 0, env, builtin_keys));

    char *len_params[] = {"list_or_dict"};
    size_t len_param_count = 1;
    declare_var(env, "len", (RuntimeVal *)MK_FUNCTION(len_params, len_param_count, NULL, 0, env, builtin_len));

    char *print_params[] = {"value"};
    size_t print_param_count = 1;
    declare_var(env, "print", (RuntimeVal *)MK_FUNCTION(print_params, print_param_count, NULL, 0, env, builtin_print_value));

    char *println_params[] = {"value"};
    size_t println_param_count = 1;
    declare_var(env, "println", (RuntimeVal *)MK_FUNCTION(println_params, println_param_count, NULL, 0, env, builtin_println_value));

    char *random_params[] = {};
    size_t random_param_count = 0;
    declare_var(env, "random", (RuntimeVal *)MK_FUNCTION(random_params, random_param_count, NULL, 0, env, builtin_random));

    char *random_int_params[] = {"min", "max"};
    size_t random_int_param_count = 2;
    declare_var(env, "random_int", (RuntimeVal *)MK_FUNCTION(random_int_params, random_int_param_count, NULL, 0, env, builtin_random_int));

    char *values_params[] = {"dict"};
    size_t values_param_count = 1;
    declare_var(env, "values", (RuntimeVal *)MK_FUNCTION(values_params, values_param_count, NULL, 0, env, builtin_values));

    char *find_params[] = {"list_or_string", "value"};
    size_t find_param_count = 2;
    declare_var(env, "find", (RuntimeVal *)MK_FUNCTION(find_params, find_param_count, NULL, 0, env, builtin_find));
}
