#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include "ast.h"
#include "builtins.h"
#include "env.h"
#include "eval.h"
#include "global.h"
#include "lexer.h"
#include "parser.h"
#include "values.h"
#include "zox_alloc.h"

#define MAX_LINE_LENGTH 1024

void run_repl(Environment *env);
void run_program(Program *program, Environment *env);

ExecutionContext global_context = {0};

static uint64_t file_mtime(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  return (uint64_t)st.st_mtime;
}

static Program *try_load_cache(const char *cache_path, uint64_t mtime) {
  FILE *f = fopen(cache_path, "rb");
  if (!f) return NULL;
  Program *program = ast_deserialize(f, mtime);
  fclose(f);
  return program;
}

static void save_cache(Program *program, const char *cache_path, uint64_t mtime) {
  FILE *f = fopen(cache_path, "wb");
  if (!f) return;
  ast_serialize(program, f, mtime);
  fclose(f);
}

static char *make_cache_path(const char *zo_path) {
  size_t len = strlen(zo_path);
  char *cache = zox_alloc_buf(ZOX_BUF_IO, len + 6, "cache_path");
  memcpy(cache, zo_path, len);
  cache[len] = '\0';
  if (len >= 3 && strcmp(cache + len - 3, ".zo") == 0) {
    memcpy(cache + len - 3, ".zoxc", 6);
  } else {
    memcpy(cache + len, ".zoxc", 6);
  }
  return cache;
}

void run_program(Program *program, Environment *env) {
  RuntimeVal *result = eval_program(program, env);
  release(result);
}

void run_repl(Environment *env) {
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  printf("Zox REPL\n");
  while (1) {
    printf(">>> ");
    read = getline(&line, &len, stdin);
    if (read == -1) break;
    if (read > 0 && line[read - 1] == '\n') line[read - 1] = '\0';
    if (strcmp(line, "exit") == 0) break;
    if (strlen(line) == 0 || (strlen(line) == 1 && line[0] == ';')) continue;

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
      release(result);
      free_program(program);
      free_tokens(tokens, token_count);
      zox_free_buf(ZOX_BUF_MISC, parser);
    }
  }
  zox_free_buf(ZOX_BUF_IO, line);
}

static void declare_const(Environment *env, const char *name, RuntimeVal *val) {
  declare_var(env, name, val);
  release(val);
}

static int parse_arena_mb(const char *value, size_t *bytes) {
  char *end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);
  if (parsed == 0 || end == value) return 0;
  if (strcmp(end, "") != 0 && strcmp(end, "MB") != 0 && strcmp(end, "mb") != 0)
    return 0;
  *bytes = (size_t)parsed * 1024 * 1024;
  return 1;
}

static int parse_cli(int argc, char **argv, const char **filename, int *alloc_stats) {
  *filename    = NULL;
  *alloc_stats = 0;

  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--arena=", 8) == 0) {
      size_t bytes = 0;
      if (!parse_arena_mb(argv[i] + 8, &bytes)) {
        fprintf(stderr, "Invalid --arena value. Use --arena=N or --arena=NMB.\n");
        return 0;
      }
      zox_arena_init(bytes);
    } else if (strcmp(argv[i], "--alloc-stats") == 0) {
      *alloc_stats = 1;
    } else if (*filename == NULL) {
      *filename = argv[i];
    } else {
      fprintf(stderr, "Usage: %s [--arena=NMB] [--alloc-stats] [file.zo]\n", argv[0]);
      return 0;
    }
  }
  return 1;
}

int main(int argc, char **argv) {
  zox_argc = argc;
  zox_argv = argv;
  const char *filename    = NULL;
  int         alloc_stats = 0;
  int         ok = parse_cli(argc, argv, &filename, &alloc_stats);

  if (!ok) return 1;

  Environment *env = create_environment(NULL, "global");
  builtins_env = env;
  register_builtins(env);
  declare_const(env, "nil",   (RuntimeVal *)MK_NIL());
  declare_const(env, "true",  (RuntimeVal *)MK_BOOL(1));
  declare_const(env, "false", (RuntimeVal *)MK_BOOL(0));
  declare_const(env, "PI",    (RuntimeVal *)MK_NUMBER(3.14159265359));

  if (!filename) {
    global_context.is_repl = 1;
    run_repl(env);
  } else {
    global_context.is_repl = 0;
    error_cursor.file = filename;
    Program *program     = NULL;
    Token   *tokens      = NULL;
    Parser  *parser      = NULL;
    size_t   token_count = 0;
    uint64_t mtime       = file_mtime(filename);
    char    *cache_path  = make_cache_path(filename);

    if (mtime > 0) {
      program = try_load_cache(cache_path, mtime);
    }

    if (!program) {
      char *source_code = read_file(filename);

      if (!source_code) {
        zox_free_buf(ZOX_BUF_IO, cache_path);
        free_environment(env);
        zox_alloc_cleanup();
        return 1;
      }

      tokens  = tokenize(source_code, &token_count);
      parser  = create_parser(tokens, token_count);
      program = produce_ast(parser, source_code);
      zox_free_buf(ZOX_BUF_IO, source_code);

      if (mtime > 0) {
        save_cache(program, cache_path, mtime);
      }
    }

    run_program(program, env);

    free_program(program);
    if (tokens) free_tokens(tokens, (int)token_count);
    if (parser) zox_free_buf(ZOX_BUF_MISC, parser);
    zox_free_buf(ZOX_BUF_IO, cache_path);
  }

  break_env_cycles(env);

  if (alloc_stats) zox_alloc_report(stderr);

  zox_alloc_cleanup();  /* drain free lists before releasing arena memory */
  zox_arena_destroy();
  return 0;
}
