# Zox Language Spec

Zox is interpreted, dynamically typed, and expression-based. Blocks return the
last evaluated value unless `_>>` returns earlier.

## Types

| Type | Literal |
|---|---|
| `nil` | `nil` |
| `boolean` | `true`, `false` |
| `number` | `42`, `3.14`, `-7` |
| `string` | `"hello"`, `'world'` |
| `list` | `{1, "two", true}` |
| `dict` | `["x" -> 1; "y" -> 2]` |
| `function` | `$ f(x) { x * 2 }` |

Global constants: `nil`, `true`, `false`, `PI`.

## Comments

Only line comments exist.

```zox
let x = 5; -# line comment
-# full-line comment
```

## Variables

```zox
let x = 10;
x = 20;
```

Collection assignment:

```zox
let xs = {1, 2, 3};
xs[0] = 99;

let d = ["k" -> "v"];
d{"k"} = "next";
```

## Operators

Arithmetic:

| Operator | Meaning |
|---|---|
| `+` | add |
| `-` | subtract |
| `*` | multiply |
| `/` | divide |
| `%` | modulo |
| `**` | power |

Comparison and logic:

| Operator | Meaning |
|---|---|
| `==`, `!=` | equality |
| `<`, `<=`, `>`, `>=` | comparison |
| `&&` | logical AND |
| `\|\|` | logical OR |

Bitwise:

| Operator | Meaning |
|---|---|
| `&` | AND |
| `\|` | OR |
| `^` | XOR |
| `<<` | shift left / list append |
| `>>` | shift right |

Strings:

| Operator | Meaning |
|---|---|
| `string + string` | concat |
| `string - string` | remove occurrences |
| `string * number` | repeat |
| `==`, `!=` | compare |

Lists:

| Operator | Meaning |
|---|---|
| `list + list` | concat |
| `list - list` | difference |
| `list * list` | Cartesian product |
| `list * number` | repeat |
| `list & list` | intersection |
| `list \| list` | union |
| `list ^ list` | symmetric difference |
| `list << value` | append, mutates list |

Element-wise list operators: `&+`, `&-`, `&*`, `&/`, `&%`.

Dicts:

| Operator | Meaning |
|---|---|
| `dict + dict` | merge, right side overwrites |

Precedence, low to high:

1. assignment `=`
2. logical OR `||`
3. logical AND `&&`
4. bitwise `|`, `^`, `&`, `<<`, `>>`
5. equality `==`, `!=`
6. comparison `<`, `<=`, `>`, `>=`
7. additive `+`, `-`
8. multiplicative `*`, `/`, `%`
9. unary `-`, `+`
10. primary literals, calls, indexing

## Control Flow

Conditional:

```zox
?(x > 10) {
    "large"
} :? (x > 0) {
    "positive"
} : {
    "zero"
}
```

While:

```zox
let i = 0;
#(i < 5) {
    println(i);
    i = i + 1
}
```

For:

```zox
@(let i = 0; i < 5; i = i + 1) {
    println(i)
}
```

Break, continue, return:

```zox
~!!  -# break
__>  -# continue
_>>  -# return nil
_>> x -# return x
```

## Functions

```zox
$ add(a, b) {
    a + b
}

println(add(2, 3)) -# 5
```

Functions are first-class and capture their definition environment.

```zox
$ make_adder(n) {
    $ adder(x) { x + n }
}

let add5 = make_adder(5);
println(add5(10)) -# 15
```

## Custom Types

Custom types are lightweight, struct-like values with named fields.

Declaration:

```zox
type Point { x, y };
```

Instantiation uses a constructor-like call:

```zox
let p = Point(3, 4);
```

Field access and assignment use dot notation:

```zox
println(p.x);
p.y = 99;
```

Fields can also be accessed by index:

```zox
println(p[0]); -# first field
p[1] = 123;
```

Introspection:

- `typeof(value)` returns a tag like `type<Point>`
- `len(value)` returns the field count

Types can be nested:

```zox
type Address { city, zip };
type Person { name, address };
let p = Person("Alice", Address("NYC", "10001"));
println(p.address.city)
```

## Scoped Arenas / Promotion Blocks

Promotion blocks use the `|{ ... }|` form.

They execute a temporary scope intended for short-lived allocations. The final
value returned with `_>>` is promoted out of the block (deep-cloned when
necessary), so the caller receives a stable value.

```zox
let x = |{
    let nested = { {1}, {2} };
    _>> nested
}|;
println(x[0][0])
```

## Result Values

Functions can return a Result-like dictionary using:

- `_>>@ expr` return an ok result
- `_>>! expr` return an err result

Propagation/unwrapping uses `!?`:

```zox
$ divide(a, b) {
    ?(b == 0) { _>>! "division by zero" } : { _>>@ a / b }
}

$ calculate(a, b, c) {
    let d = divide(a, b)!?; -# unwrap ok or early-return err
    _>>@ d + c
}
```

Common helpers include: `ok`, `err`, `is_ok`, `is_err`, `get_ok`, `get_err`.

## Pattern Matching

Pattern matching uses `?? (value) { ... }` with `=>` arms and `_` as a wildcard.

```zox
let x = 2;
let res = ?? (x) {
    1 => "one",
    2 => "two",
    _ => "unknown"
};
println(res)
```

Arms can also be boolean guards:

```zox
let age = 20;
let group = ?? (age) {
    age >= 65 => "senior",
    age >= 18 => "adult",
    _ => "minor"
};
println(group)
```

## Collections

Lists:

```zox
let xs = {10, 20, 30, 40};
println(xs[0]);    -# 10
println(xs[-1]);   -# 40
println(xs[1:3]);  -# {20, 30}
println(xs[:2]);   -# {10, 20}
println(xs[2:]);   -# {30, 40}
```

Dicts:

```zox
let d = ["x" -> 100; "y" -> 200];
println(d{"x"}); -# 100
d{"z"} = 300;
```

## Imports

```zox
~> math {abs, sqrt as root};
~> file {open, fRead, fClose};
~> "./lib/json.so" {parse, stringify};
```

Lookup order:

1. native modules
2. direct `.zo`, `.so`, `.dll` path
3. `./`, `./lib`, `./packages`, `/usr/local/lib/zox/packages`

Dot names map to paths: `foo.bar` searches as `foo/bar`.

## Builtins

| Function | Summary |
|---|---|
| `print(value)` | print without newline |
| `println(value)` | print with newline |
| `len(value)` | string/list/dict length |
| `keys(dict)` | dict keys |
| `values(dict)` | dict values |
| `sum(list)` | numeric list sum |
| `find(target, value)` | string/list index or dict key presence |
| `copy(value)` | deep-copy lists/dicts |
| `typeof(value)` | runtime type name |
| `random()` | float in `[0, 1)` |
| `random_int(min, max)` | integer in `[min, max]` |

## Native Modules

`math`:

`abs`, `sqrt`, `sin`, `cos`, `tan`, `log`, `pow`, `floor`, `ceil`, `round`,
`min`, `max`, `lmin`, `lmax`, `average`, `median`.

`file`:

`open`, `fRead`, `fReadLine`, `fWrite`, `fSeek`, `fClose`, `fExists`,
`fDelete`, `fCopy`, `fMove`.

`string`:

`upper`, `lower`, `trim`, `startsWith`, `endsWith`, `replace`, `split`,
`join`, `toNumber`, `toString`.

`os`:

`exit`, `env`, `args`, `exec`.

## Library Modules

Dynamic C modules in `lib/`:

| Module | Functions |
|---|---|
| `collections` | `range`, `zip`, `flatten`, `unique`, `chunk`, `count`, `group_by` |
| `functional` | `map`, `filter`, `reduce`, `any`, `all`, `take`, `drop`, `zip_with`, `pipe` |
| `json` | `parse`, `stringify` |

Pure Zox module:

| Module | Functions |
|---|---|
| `assert` | `assert_true`, `assert_false`, `assert_eq`, `assert_neq`, `assert_lt`, `assert_gt`, `assert_lte`, `assert_gte`, `assert_in`, `assert_not_in`, `assert_len`, `assert_contains`, `assert_summary`, `assert_type` |

## Coercion

- Booleans can participate in numeric contexts: `true -> 1`, `false -> 0`.
- Strings and numbers do not coerce automatically.

## Examples

```zox
$ factorial(n) {
    ?(n <= 1) {
        _>> 1
    }
    n * factorial(n - 1)
}

println(factorial(10)) -# 3628800
```

```zox
~> math {sqrt, abs};

let data = ["val" -> -9; "label" -> "root"];
let v = abs(data{"val"});
println(data{"label"} + ": " + sqrt(v)); -# root: 3
```
