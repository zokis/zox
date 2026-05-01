# Zox

Zox is a small interpreted, expression-based language written in C.

It exists as an educational interpreter: lexer, parser, AST, evaluator,
scopes, native modules, dynamic modules, and manual memory management are kept
visible and hackable.

## Build

```bash
make          # release build
make dev      # AddressSanitizer build
make libs     # build lib/*.so dynamic modules
make full     # core + dynamic modules
make test     # core test suite
make testlibs # dynamic module tests
make clean
```

Manual build:

```bash
gcc -O2 -o zox \
  main.c ast/ast_nodes.c ast/ast_free.c ast/ast_serial.c \
  lexer.c parser/parser_core.c parser/parser_exprs.c parser/parser_stmts.c \
  values.c eval/eval_core.c eval/eval_ops.c eval/eval_control.c \
  eval/eval_collections.c eval/eval_funcs.c eval/eval_import.c \
  malloc_safe.c env.c debug.c hash.c builtins.c global.c native_modules.c \
  -lm -ldl -Wl,--export-dynamic
```

## Run

```bash
./zox              # REPL
./zox file.zo      # run file
```

REPL:

```text
Zox REPL
>>> $ square(n) { n * n }
<function>
>>> square(4)
16
>>> let xs = {1, 2, 3};
{1, 2, 3}
>>> xs[1]
2
```

## Language

Zox is dynamically typed. Blocks return their last evaluated value.

| Type | Literal |
|---|---|
| `nil` | `nil` |
| `boolean` | `true`, `false` |
| `number` | `42`, `3.14`, `-7` |
| `string` | `"hello"`, `'world'` |
| `list` | `{1, "two", true}` |
| `dict` | `["x" -> 1; "y" -> 2]` |
| `function` | `$ f(x) { x * 2 }` |

## Syntax

Variables:

```zox
let name = "Zox";
let count = 3;
count = count + 1;
```

Functions:

```zox
$ add(a, b) {
    a + b
}

println(add(2, 3)) -# 5
```

Closures:

```zox
$ make_adder(n) {
    $ adder(x) { x + n }
}

let add5 = make_adder(5);
println(add5(10)) -# 15
```

Conditionals:

```zox
?(count > 10) {
    "large"
} :? (count > 0) {
    "positive"
} : {
    "zero"
}
```

Loops:

```zox
let i = 0;
#(i < 5) {
    println(i);
    i = i + 1
}

@(let j = 0; j < 5; j = j + 1) {
    println(j)
}
```

Control flow:

```zox
~!!  -# break
__>  -# continue
_>>  -# return
```

Comments:

```zox
let x = 5; -# line comment
-# full-line comment
```

## Collections

Lists use `{}`:

```zox
let xs = {1, 2, 3};
println(xs[0]);   -# 1
println(xs[-1]);  -# 3
println(xs[1:]);  -# {2, 3}

xs << 4;
println(xs);      -# {1, 2, 3, 4}
```

Dicts use `[]` and key access uses `{}`:

```zox
let user = ["name" -> "Ana"; "age" -> 28];
println(user{"name"}); -# Ana
user{"age"} = 29;
```

Collection operators:

| Operator | Meaning |
|---|---|
| `list + list` | concat |
| `list - list` | difference |
| `list * list` | Cartesian product |
| `list * n` | repeat |
| `list & list` | intersection |
| `list \| list` | union |
| `list ^ list` | symmetric difference |
| `dict + dict` | merge, right wins |
| `list &+ n` | element-wise add |
| `list &- n` | element-wise subtract |
| `list &* n` | element-wise multiply |
| `list &/ n` | element-wise divide |
| `list &% n` | element-wise modulo |

## Imports

```zox
~> math {abs, sqrt as root};
println(abs(-16));  -# 16
println(root(16));  -# 4
```

Import sources:

| Source | Examples |
|---|---|
| Native modules | `math`, `file`, `string`, `os` |
| Zox modules | `./`, `./lib/`, `./packages/` |
| Dynamic modules | `.so`, `.dll` path imports |

Dynamic module example:

```zox
~> "./lib/json.so" {parse, stringify};
let data = parse("{\"ok\":true}");
println(data{"ok"});
```

## Builtins

| Function | Summary |
|---|---|
| `print(value)` | print without newline |
| `println(value)` | print with newline |
| `len(value)` | string/list/dict length |
| `keys(dict)` | dict keys |
| `values(dict)` | dict values |
| `sum(list)` | numeric list sum |
| `find(target, value)` | index/key lookup, `-1` if missing |
| `copy(value)` | deep-copy lists/dicts |
| `typeof(value)` | runtime type name |
| `random()` | float in `[0, 1)` |
| `random_int(min, max)` | integer in `[min, max]` |

## Modules

Native modules:

| Module | Functions |
|---|---|
| `math` | `abs`, `sqrt`, `sin`, `cos`, `tan`, `log`, `pow`, `floor`, `ceil`, `round`, `min`, `max`, `lmin`, `lmax`, `average`, `median` |
| `file` | `open`, `fRead`, `fReadLine`, `fWrite`, `fSeek`, `fClose`, `fExists`, `fDelete`, `fCopy`, `fMove` |
| `string` | `upper`, `lower`, `trim`, `startsWith`, `endsWith`, `replace`, `split`, `join`, `toNumber`, `toString` |
| `os` | `exit`, `env`, `args`, `exec` |

Dynamic modules in `lib/`:

| Module | Functions |
|---|---|
| `collections` | `range`, `zip`, `flatten`, `unique`, `chunk`, `count`, `group_by` |
| `functional` | `map`, `filter`, `reduce`, `any`, `all`, `take`, `drop`, `zip_with`, `pipe` |
| `json` | `parse`, `stringify` |
| `assert` | pure Zox assertion helpers |

## Architecture

Pipeline:

```text
source -> lexer -> parser -> AST -> evaluator -> RuntimeVal
                              |
                              v
                         Environment
```

Main areas:

| Area | Files |
|---|---|
| Lexer | `lexer.c`, `lexer.h` |
| Parser | `parser/*.c`, `parser/parser_internal.h`, `parser.h` |
| AST | `ast/*.c`, `ast.h` |
| Evaluator | `eval/*.c`, `eval/eval_internal.h`, `eval.h` |
| Runtime values | `values.c`, `values.h` |
| Environment | `env.c`, `env.h` |
| Builtins | `builtins.c`, `builtins.h` |
| Native modules | `native_modules.c`, `native_modules/*.c` |
| Dynamic module API | `zox_module.h` |

Runtime ownership:

- `RuntimeVal` and `Environment` use manual reference counting.
- `nil`, booleans, and integers `0..255` are static singletons.
- `evaluate()` returns values with caller ownership.
- `declare_owned()` stores newly-created values without leaking the creator ref.
- `break_env_cycles()` releases captured env cycles before shutdown.

## Docs

- [`specs/language-spec.md`](specs/language-spec.md)
- [`specs/architecture.md`](specs/architecture.md)

## Examples

```bash
make full
./zox examples/json_example.zo
./zox examples/log_analyzer.zo
```

## Tests

```bash
make test
make testlibs
```
