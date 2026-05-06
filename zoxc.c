#include <limits.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ast.h"
#include "codegen.h"
#include "global.h"
#include "lexer.h"
#include "parser.h"
#include "zox_alloc.h"

ExecutionContext global_context = {0};

typedef struct {
    const char *input;
    const char *output;
    const char *asm_output;
    int strip;
    int keep_asm;
} CompilerOptions;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <file.zo> [-o output] [--emit-asm file.s] [--strip]\n", prog);
}

static int parse_cli(int argc, char **argv, CompilerOptions *opts) {
    opts->input = NULL;
    opts->output = "a.out";
    opts->asm_output = NULL;
    opts->strip = 0;
    opts->keep_asm = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            opts->output = argv[++i];
        } else if (strcmp(argv[i], "--emit-asm") == 0 && i + 1 < argc) {
            opts->asm_output = argv[++i];
            opts->keep_asm = 1;
        } else if (strcmp(argv[i], "--strip") == 0) {
            opts->strip = 1;
        } else if (!opts->input) {
            opts->input = argv[i];
        } else {
            usage(argv[0]);
            return 0;
        }
    }
    if (!opts->input) {
        usage(argv[0]);
        return 0;
    }
    return 1;
}

static int dirname_from_path(const char *path, char *out, size_t out_size) {
    const char *slash = strrchr(path, '/');
    size_t len;

    if (!path || !out || out_size == 0) return 0;
    if (!slash) {
        if (out_size < 2) return 0;
        out[0] = '.';
        out[1] = '\0';
        return 1;
    }

    len = (slash == path) ? 1u : (size_t)(slash - path);
    if (len + 1 > out_size) return 0;
    memcpy(out, path, len);
    out[len] = '\0';
    return 1;
}

static int resolve_runtime_dir(const char *argv0, char *dir, size_t dir_size) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);

    if (len > 0) {
        exe_path[len] = '\0';
        if (dirname_from_path(exe_path, dir, dir_size)) return 1;
    }

    if (argv0 && strchr(argv0, '/')) {
        char full_path[PATH_MAX];
        if (realpath(argv0, full_path) && dirname_from_path(full_path, dir, dir_size)) return 1;
    }

    return dirname_from_path(".", dir, dir_size);
}

static int append_fmt(char *buf, size_t buf_size, size_t *len, const char *fmt, ...) {
    va_list args;
    int written;

    if (*len >= buf_size) return 0;

    va_start(args, fmt);
    written = vsnprintf(buf + *len, buf_size - *len, fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= buf_size - *len) return 0;
    *len += (size_t)written;
    return 1;
}

static int run_process(char *const argv[], const char *failure_message) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        fprintf(stderr, "%s\n", failure_message);
        return 0;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        fprintf(stderr, "%s\n", failure_message);
        return 0;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 1;
    fprintf(stderr, "%s\n", failure_message);
    return 0;
}

static int join_path(char *out, size_t out_size, const char *dir, const char *rel) {
    return snprintf(out, out_size, "%s/%s", dir, rel) < (int)out_size;
}

static int make_temp_path(const char *suffix, char *path, size_t path_size) {
    int fd;
    const char *tmp_dir = getenv("TMPDIR");
    char template_path[PATH_MAX];
    int suffix_len = (int)strlen(suffix);

    if (!tmp_dir || tmp_dir[0] == '\0') tmp_dir = "/tmp";
    if (snprintf(template_path, sizeof(template_path), "%s/zoxcXXXXXX%s", tmp_dir, suffix) >= (int)sizeof(template_path)) {
        return 0;
    }
    if (snprintf(path, path_size, "%s", template_path) >= (int)path_size) return 0;
    fd = mkstemps(path, suffix_len);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

int main(int argc, char **argv) {
    CompilerOptions opts;
    char            runtime_dir[PATH_MAX];
    char            tmp_s[PATH_MAX];
    char            tmp_o[PATH_MAX];
    char            runtime_lib[PATH_MAX];
    char            ast_nodes[PATH_MAX];
    char            ast_free[PATH_MAX];
    char            ast_serial[PATH_MAX];
    char            lexer_path[PATH_MAX];
    char            parser_core[PATH_MAX];
    char            parser_exprs[PATH_MAX];
    char            parser_stmts[PATH_MAX];
    char            values_path[PATH_MAX];
    char            eval_core[PATH_MAX];
    char            eval_ops[PATH_MAX];
    char            eval_control[PATH_MAX];
    char            eval_collections[PATH_MAX];
    char            eval_funcs[PATH_MAX];
    char            eval_import[PATH_MAX];
    char            malloc_safe[PATH_MAX];
    char            zox_alloc_path[PATH_MAX];
    char            env_path[PATH_MAX];
    char            debug_path[PATH_MAX];
    char            hash_path[PATH_MAX];
    char            builtins_path[PATH_MAX];
    char            global_path[PATH_MAX];
    char            native_modules[PATH_MAX];
    const char     *asm_path = NULL;

    if (!parse_cli(argc, argv, &opts)) return 1;

    error_cursor.file      = opts.input;
    global_context.is_repl = 0;

    char *source = read_file(opts.input);
    if (!source) {
        fprintf(stderr, "Error: cannot read '%s'\n", opts.input);
        return 1;
    }

    size_t   token_count;
    Token   *tokens  = tokenize(source, &token_count);
    Parser  *parser  = create_parser(tokens, token_count);
    Program *program = produce_ast(parser, source);
    zox_free_buf(ZOX_BUF_IO, source);

    if (!make_temp_path(".s", tmp_s, sizeof(tmp_s)) ||
        !make_temp_path(".o", tmp_o, sizeof(tmp_o))) {
        fprintf(stderr, "Error: cannot create temporary file names\n");
        free_program(program);
        free_tokens(tokens, (int)token_count);
        zox_free_buf(ZOX_BUF_MISC, parser);
        zox_alloc_cleanup();
        zox_arena_destroy();
        return 1;
    }
    asm_path = opts.keep_asm ? opts.asm_output : tmp_s;

    FILE *asm_out = fopen(asm_path, "w");
    if (!asm_out) {
        fprintf(stderr, "Error: cannot create %s\n", asm_path);
        free_program(program);
        free_tokens(tokens, (int)token_count);
        zox_free_buf(ZOX_BUF_MISC, parser);
        zox_alloc_cleanup();
        zox_arena_destroy();
        return 1;
    }
    Codegen *cg = create_codegen(asm_out);
    codegen_program(cg, program);
    free_codegen(cg);
    fclose(asm_out);

    free_program(program);
    free_tokens(tokens, (int)token_count);
    zox_free_buf(ZOX_BUF_MISC, parser);
    zox_alloc_cleanup();
    zox_arena_destroy();

    if (!resolve_runtime_dir(argv[0], runtime_dir, sizeof(runtime_dir))) {
        fprintf(stderr, "Error: cannot resolve runtime directory\n");
        if (!opts.keep_asm) unlink(tmp_s);
        return 1;
    }

    {
        char *nasm_argv[] = { "nasm", "-f", "elf64", (char *)asm_path, "-o", tmp_o, NULL };
        if (!run_process(nasm_argv, "Error: assembly failed")) {
            if (!opts.keep_asm) unlink(tmp_s);
            unlink(tmp_o);
            return 1;
        }
    }

    if (!join_path(runtime_lib, sizeof(runtime_lib), runtime_dir, "runtime_lib.c") ||
        !join_path(ast_nodes, sizeof(ast_nodes), runtime_dir, "ast/ast_nodes.c") ||
        !join_path(ast_free, sizeof(ast_free), runtime_dir, "ast/ast_free.c") ||
        !join_path(ast_serial, sizeof(ast_serial), runtime_dir, "ast/ast_serial.c") ||
        !join_path(lexer_path, sizeof(lexer_path), runtime_dir, "lexer.c") ||
        !join_path(parser_core, sizeof(parser_core), runtime_dir, "parser/parser_core.c") ||
        !join_path(parser_exprs, sizeof(parser_exprs), runtime_dir, "parser/parser_exprs.c") ||
        !join_path(parser_stmts, sizeof(parser_stmts), runtime_dir, "parser/parser_stmts.c") ||
        !join_path(values_path, sizeof(values_path), runtime_dir, "values.c") ||
        !join_path(eval_core, sizeof(eval_core), runtime_dir, "eval/eval_core.c") ||
        !join_path(eval_ops, sizeof(eval_ops), runtime_dir, "eval/eval_ops.c") ||
        !join_path(eval_control, sizeof(eval_control), runtime_dir, "eval/eval_control.c") ||
        !join_path(eval_collections, sizeof(eval_collections), runtime_dir, "eval/eval_collections.c") ||
        !join_path(eval_funcs, sizeof(eval_funcs), runtime_dir, "eval/eval_funcs.c") ||
        !join_path(eval_import, sizeof(eval_import), runtime_dir, "eval/eval_import.c") ||
        !join_path(malloc_safe, sizeof(malloc_safe), runtime_dir, "malloc_safe.c") ||
        !join_path(zox_alloc_path, sizeof(zox_alloc_path), runtime_dir, "zox_alloc.c") ||
        !join_path(env_path, sizeof(env_path), runtime_dir, "env.c") ||
        !join_path(debug_path, sizeof(debug_path), runtime_dir, "debug.c") ||
        !join_path(hash_path, sizeof(hash_path), runtime_dir, "hash.c") ||
        !join_path(builtins_path, sizeof(builtins_path), runtime_dir, "builtins.c") ||
        !join_path(global_path, sizeof(global_path), runtime_dir, "global.c") ||
        !join_path(native_modules, sizeof(native_modules), runtime_dir, "native_modules.c")) {
        fprintf(stderr, "Error: runtime path overflow\n");
        if (!opts.keep_asm) unlink(tmp_s);
        unlink(tmp_o);
        return 1;
    }

    {
        char *gcc_argv[40];
        int argc_gcc = 0;
        gcc_argv[argc_gcc++] = "gcc";
        gcc_argv[argc_gcc++] = "-no-pie";
        if (opts.strip) gcc_argv[argc_gcc++] = "-s";
        gcc_argv[argc_gcc++] = tmp_o;
        gcc_argv[argc_gcc++] = runtime_lib;
        gcc_argv[argc_gcc++] = ast_nodes;
        gcc_argv[argc_gcc++] = ast_free;
        gcc_argv[argc_gcc++] = ast_serial;
        gcc_argv[argc_gcc++] = lexer_path;
        gcc_argv[argc_gcc++] = parser_core;
        gcc_argv[argc_gcc++] = parser_exprs;
        gcc_argv[argc_gcc++] = parser_stmts;
        gcc_argv[argc_gcc++] = values_path;
        gcc_argv[argc_gcc++] = eval_core;
        gcc_argv[argc_gcc++] = eval_ops;
        gcc_argv[argc_gcc++] = eval_control;
        gcc_argv[argc_gcc++] = eval_collections;
        gcc_argv[argc_gcc++] = eval_funcs;
        gcc_argv[argc_gcc++] = eval_import;
        gcc_argv[argc_gcc++] = malloc_safe;
        gcc_argv[argc_gcc++] = zox_alloc_path;
        gcc_argv[argc_gcc++] = env_path;
        gcc_argv[argc_gcc++] = debug_path;
        gcc_argv[argc_gcc++] = hash_path;
        gcc_argv[argc_gcc++] = builtins_path;
        gcc_argv[argc_gcc++] = global_path;
        gcc_argv[argc_gcc++] = native_modules;
        gcc_argv[argc_gcc++] = "-lm";
        gcc_argv[argc_gcc++] = "-ldl";
        gcc_argv[argc_gcc++] = "-rdynamic";
        gcc_argv[argc_gcc++] = "-o";
        gcc_argv[argc_gcc++] = (char *)opts.output;
        gcc_argv[argc_gcc] = NULL;

        if (!run_process(gcc_argv, "Error: linking failed")) {
            if (!opts.keep_asm) unlink(tmp_s);
            unlink(tmp_o);
            return 1;
        }
    }

    if (!opts.keep_asm) unlink(tmp_s);
    unlink(tmp_o);

    printf("Done! Binary: %s\n", opts.output);
    if (opts.keep_asm) printf("Assembly: %s\n", opts.asm_output);
    return 0;
}
