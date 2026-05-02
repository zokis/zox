# Zox Architecture

Zox is a tree-walking interpreter written in C.

```text
source -> lexer -> parser -> AST -> evaluator -> RuntimeVal
                              |
                              v
                         Environment
```

## Build Units

| Area | Files |
|---|---|
| Entry point | `main.c` |
| Lexer | `lexer.c`, `lexer.h` |
| Parser | `parser/parser_core.c`, `parser/parser_exprs.c`, `parser/parser_stmts.c`, `parser/parser_internal.h`, `parser.h` |
| AST | `ast/ast_nodes.c`, `ast/ast_free.c`, `ast/ast_serial.c`, `ast.h` |
| Evaluator | `eval/eval_core.c`, `eval/eval_ops.c`, `eval/eval_control.c`, `eval/eval_collections.c`, `eval/eval_funcs.c`, `eval/eval_import.c`, `eval/eval_internal.h`, `eval.h` |
| Runtime values | `values.c`, `values.h` |
| Environment | `env.c`, `env.h` |
| Builtins | `builtins.c`, `builtins.h` |
| Native modules | `native_modules.c`, `native_modules/*.c`, `native_modules.h` |
| Dynamic module API | `zox_module.h` |
| Utilities | `global.c`, `global.h`, `hash.c`, `hash.h`, `malloc_safe.c`, `malloc_safe.h`, `zox_alloc.c`, `zox_alloc.h`, `debug.c` |

## Runtime Pipeline

File execution:

1. `main()` creates global `Environment`.
2. `register_builtins()` installs always-available functions.
3. Source file mtime selects `.zoxc` cache or parse path.
4. `tokenize()` produces `Token[]`.
5. `create_parser()` stores token stream.
6. `produce_ast()` creates `Program`.
7. `eval_program()` evaluates statements.
8. `break_env_cycles()` releases captured-env cycles.
9. `free_environment()` releases global scope.

REPL:

1. Read line.
2. Tokenize, parse, evaluate.
3. Print non-`nil` result.
4. `longjmp` returns to prompt after runtime errors.

## Lexer

The lexer converts source text into `Token[]`.

It recognizes:

- numbers
- single-quoted and double-quoted strings
- identifiers, including UTF-8 identifier chars
- keywords and symbolic forms: `let`, `$`, `?`, `:?`, `@`, `#`, `~>`,
  `type`, `~!!`, `__>`, `_>>`, `_>>@`, `_>>!`, `??`, `!?`, `=>`, `|{`, `}|`
- comments starting with `-#`
- operators, delimiters, import aliases

Each token carries value, type, line, and column.

## Parser

The parser is recursive descent.

Core entry points:

- `create_parser()`
- `produce_ast()`
- `parse_stmt()`
- `parse_expr()`

Precedence chain:

```text
assignment -> logical_or -> logical_and -> equality -> comparison
-> additive -> bitwise -> multiplicative -> unary -> primary
```

Statement parsing handles imports, `let`, `~!!`, `__>`, `_>>`, and expression
statements with optional semicolons.

## AST

AST nodes share `Stmt` / `Expr` headers and use `NodeType` tags.

Important node families:

- literals: nil, boolean, number, string, list, dict
- identifiers and assignment
- binary and unary expressions
- control flow: if, while, for, break, continue, return
- functions and calls
- imports
- list/dict indexing and assignment

`ast/ast_serial.c` writes and reads `.zoxc` cache files. Cache invalidates when
stored source mtime differs from current source mtime.

## Evaluator

`evaluate(Stmt *node, Environment *env)` dispatches by `NodeType`.

Evaluation rules:

- values return with caller ownership
- binary expressions evaluate both sides, compute result, release operands
- function calls create child envs from captured definition env
- builtins call C function pointers directly
- pure Zox functions evaluate body statements in function env

Control flow uses global signal state:

```c
typedef enum { CF_NONE = 0, CF_BREAK, CF_CONTINUE, CF_RETURN } ControlFlowKind;

ControlFlowKind cf_signal;
RuntimeVal *cf_return_val;
```

Loops consume `CF_BREAK` and `CF_CONTINUE`. Function calls consume `CF_RETURN`.
Nested callers propagate unhandled signals.

## Runtime Values

`RuntimeVal` is the common header:

```c
typedef struct {
  ValueType type;
  int ref_count;
} RuntimeVal;
```

Concrete values:

- `NilVal`
- `BooleanVal`
- `NumberVal`
- `StringVal`
- `ListVal`
- `DictVal`
- `FunctionVal`

Static singletons:

- `nil`
- `true`
- `false`
- integers `0..255`

Singletons use `ref_count = -1`; `retain()` and `release()` skip them.

Runtime allocation (`zox_alloc.c`):

- pooled fixed-size structs: `NumberVal`, `StringVal`, `ListVal`, `DictVal`, `Entry`, `FunctionVal`, `Environment`
- not pooled: string contents, list item arrays, dict bucket arrays, AST arrays, token arrays
- `--arena=NMB` pre-allocates N MB as a general backing arena for all pooled types:
  - first allocation of each object bumps from the arena
  - freed arena-owned objects return to the per-type free list for reuse
  - peak allocations that exceed the arena go to `malloc` and are freed immediately on release — they do not accumulate in the free list
- without `--arena`: per-type free lists with a cap of 64 objects; excess freed immediately
- `--alloc-stats` prints per-kind counters (alloc, reuse, arena, heap, freed, pooled, depth) and arena usage to stderr

Arena sizing:

- `--arena` is a tuning knob, not a guaranteed speedup.
- A too-small arena can be slower than no arena. Once the arena fills, overflow
  allocations fall back to `malloc`, so the run may pay both arena bookkeeping
  and heap allocation costs.
- To estimate a useful size, run the target program with a deliberately large
  arena and allocation stats:

```bash
./zox --arena=64MB --alloc-stats program.zo
```

- Use the final `arena used=... / total=...` line to choose a smaller value
  with headroom. For example, `arena used=6233kB / total=65536kB` suggests
  trying `--arena=8MB` or `--arena=12MB`.
- Avoid sizes where usage is at or near the limit, such as
  `arena used=4095kB / total=4096kB`; that indicates exhaustion or near
  exhaustion and should be benchmarked against running without an arena.
- Validate candidate sizes with repeated timing and RSS measurements, for
  example:

```bash
RUNS=30 ARENA=8MB scripts/compare_arena.sh program.zo
RUNS=30 ARENA=16MB scripts/compare_arena.sh program.zo
```

## Environment

`Environment` stores scoped variables with open addressing.

```c
struct Environment {
  Environment *parent;
  HashEntry *entries;
  size_t capacity;
  size_t size;
  char *scope_name;
  int ref_count;
  void *owned_program;
  void **so_handles;
  size_t so_handle_count;
};
```

Responsibilities:

- variable declaration and assignment
- parent-scope resolution
- environment reference counting
- imported module AST ownership
- dynamic module handle ownership
- captured-env cycle breaking


## Ownership Contract

Runtime values and environments use manual reference counting.

| Operation | Ownership |
|---|---|
| `evaluate()` | returns caller-owned value |
| `declare_var()` | retains stored value |
| `declare_owned()` | declares then releases creator ref |
| `assign_var()` | releases old value, retains new value |
| `list_append_val()` | retains appended item |
| `dict_set_val()` | retains stored value |
| `MK_FUNCTION(..., env, ...)` | retains captured env |
| `free_runtime_val(FUNCTION_T)` | releases captured env |

Cycle handling:

- closures retain definition envs
- imported module envs can contain private functions capturing same env
- `break_env_cycles()` walks reachable values and clears captured env refs
  before final shutdown

## Imports

Import resolution lives in `eval/eval_import.c`.

Resolution order:

1. native module name
2. direct `.zo`, `.so`, `.dll` path
3. search paths: `.`, `./lib`, `./packages`, `/usr/local/lib/zox/packages`

Import kinds:

| Kind | Behavior |
|---|---|
| native | call registered `init_*_module()` into temporary module env |
| `.zo` | tokenize, parse, evaluate module source, keep AST in `owned_program` |
| `.so` / `.dll` | load dynamic module, call `zox_init_module`, keep handle in parent env |

Dynamic modules use `zox_module.h` and must export:

```c
void zox_init_module(Environment *env);
```

## Builtins and Modules

Builtins are registered in `register_builtins()` with `declare_owned()`.

Native modules:

- `math`
- `file`
- `string`
- `os`

Dynamic modules in `lib/`:

- `collections`
- `functional`
- `json`

Pure Zox module:

- `lib/assert.zo`

## Errors

Errors use `setjmp` / `longjmp` through `ExecutionContext`.

File mode aborts current execution. REPL mode prints the error and returns to
the prompt.

`error_cursor` is updated by the parser as tokens are consumed so runtime
errors can report useful source positions.

## Cache

File execution uses `.zoxc` binary AST cache:

- cache path appends/replaces `.zo` with `.zoxc`
- cache stores magic, version, source mtime, serialized AST
- source mtime mismatch invalidates cache
