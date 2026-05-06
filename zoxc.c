#include <limits.h>
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

static int run_command(const char *cmd, const char *failure_message) {
    if (system(cmd) == 0) return 1;
    fprintf(stderr, "%s\n", failure_message);
    return 0;
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

static int make_temp_path(const char *suffix, char *path, size_t path_size) {
    int fd;
    size_t suffix_len = strlen(suffix);
    char template_path[] = "/tmp/zoxcXXXXXX";

    if (snprintf(path, path_size, "%s", template_path) >= (int)path_size) return 0;
    fd = mkstemp(path);
    if (fd < 0) return 0;
    close(fd);
    unlink(path);

    if (strlen(path) + suffix_len + 1 > path_size) return 0;
    strcat(path, suffix);
    return 1;
}

int main(int argc, char **argv) {
    CompilerOptions opts;
    char            runtime_dir[PATH_MAX];
    char            tmp_s[64];
    char            tmp_o[64];
    char            cmd[32768];
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

    snprintf(cmd, sizeof(cmd), "nasm -f elf64 %s -o %s", asm_path, tmp_o);
    if (!run_command(cmd, "Error: assembly failed")) {
        if (!opts.keep_asm) unlink(tmp_s);
        return 1;
    }

    size_t cmd_len = 0;
    if (!append_fmt(cmd, sizeof(cmd), &cmd_len, "gcc -no-pie ")) {
        fprintf(stderr, "Error: link command overflow\n");
        if (!opts.keep_asm) unlink(tmp_s);
        unlink(tmp_o);
        return 1;
    }
    if (opts.strip && !append_fmt(cmd, sizeof(cmd), &cmd_len, "-s ")) {
        fprintf(stderr, "Error: link command overflow\n");
        if (!opts.keep_asm) unlink(tmp_s);
        unlink(tmp_o);
        return 1;
    }
    if (!append_fmt(cmd, sizeof(cmd), &cmd_len,
        "%s %s/runtime_lib.c "
        "%s/ast/ast_nodes.c %s/ast/ast_free.c %s/ast/ast_serial.c "
        "%s/lexer.c "
        "%s/parser/parser_core.c %s/parser/parser_exprs.c %s/parser/parser_stmts.c "
        "%s/values.c "
        "%s/eval/eval_core.c %s/eval/eval_ops.c %s/eval/eval_control.c "
        "%s/eval/eval_collections.c %s/eval/eval_funcs.c %s/eval/eval_import.c "
        "%s/malloc_safe.c %s/zox_alloc.c %s/env.c %s/debug.c %s/hash.c "
        "%s/builtins.c %s/global.c %s/native_modules.c "
        "-lm -ldl -rdynamic -o %s",
        tmp_o,
        runtime_dir,
        runtime_dir, runtime_dir, runtime_dir,
        runtime_dir,
        runtime_dir, runtime_dir, runtime_dir,
        runtime_dir,
        runtime_dir, runtime_dir, runtime_dir,
        runtime_dir, runtime_dir, runtime_dir,
        runtime_dir, runtime_dir, runtime_dir, runtime_dir, runtime_dir,
        runtime_dir, runtime_dir, runtime_dir,
        opts.output)) {
        fprintf(stderr, "Error: link command overflow\n");
        if (!opts.keep_asm) unlink(tmp_s);
        unlink(tmp_o);
        return 1;
    }

    int ret = run_command(cmd, "Error: linking failed");
    if (!opts.keep_asm) unlink(tmp_s);
    unlink(tmp_o);

    if (!ret) return 1;

    printf("Done! Binary: %s\n", opts.output);
    if (opts.keep_asm) printf("Assembly: %s\n", opts.asm_output);
    return 0;
}
