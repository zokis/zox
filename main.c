
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "global.h"
#include "eval.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "env.h"
#include "builtins.h"
#include "malloc_safe.h"

#define MAX_LINE_LENGTH 1024

void run_repl(Environment *env);
void run_file(const char *source_code, Environment *env);


ExecutionContext global_context = {0};


void run_file(const char *source_code, Environment *env) {
    size_t token_count;
    Token *tokens = tokenize(source_code, &token_count);
    Parser *parser = create_parser(tokens, token_count);
    Program *program = produce_ast(parser, source_code);
    eval_program(program, env);
    free_program(program);
    free_tokens(tokens, token_count);
    free_safe(parser);
}

void run_repl(Environment *env) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    printf("Zox REPL\n");
    while (1) {
        printf(">>> ");
        read = getline(&line, &len, stdin);
        if (read == -1) {
            break;
        }
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        if (strcmp(line, "exit") == 0) {
            break;
        }
        if (strlen(line) == 0 || (strlen(line) == 1 && line[0] == ';')) {
            continue;
        }
        if (setjmp(global_context.error_jmp) == 0) {
            size_t token_count;
            Token *tokens = tokenize(line, &token_count);
            Parser *parser = create_parser(tokens, token_count);
            Program *program = produce_ast(parser, line);
            RuntimeVal *result = eval_program(program, env);
            if (result->type != NIL_T) {
                RuntimeVal *args[] = {result};
                builtin_println_value(env, args, 1);
            }
            free_program(program);
            free_tokens(tokens, token_count);
            free_safe(parser);
        }
    }
    free_safe(line);
}

int main(int argc, char **argv) {
    Environment *env = create_environment(NULL, "global");
    register_builtins(env);
    declare_var(env, "nil", (RuntimeVal *)MK_NIL());
    declare_var(env, "true", (RuntimeVal *)MK_BOOL(1));
    declare_var(env, "false", (RuntimeVal *)MK_BOOL(0));
    declare_var(env, "PI", (RuntimeVal *)MK_NUMBER(3.14159265359));

    if (argc < 2) {
        global_context.is_repl = 1;
        run_repl(env);
    } else {
        global_context.is_repl = 0;
        const char *filename = argv[1];
        char *source_code = read_file(filename);
        if (!source_code) {
            return 1;
        }
        run_file(source_code, env);
        free_safe(source_code);
    }

    free_environment(env);
    return 0;
}
