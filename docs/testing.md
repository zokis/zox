# Zox Testing Guide

## Overview

Zox has two test suites:

1. **Legacy Regression Tests** (`tests/`) - print-based tests with expected output files
2. **Unit Tests** (`tests/unit/`) - modern assertion-based tests using `~> test` framework

New tests should use the unit test framework. Legacy tests are maintained for backward compatibility and regression testing.

## Unit Testing Framework

The unit test framework uses the `~> test` module for structured assertions and test organization.

### Basic Structure

```zox
~> test { test, suite,
           assert_eq, assert_true, assert_false,
           test_summary };

suite("Feature name");

$ test_function_name() {
    -# test code
    assert_eq(result, expected)
}

test("description", test_function_name);

test_summary()
```

### Running Unit Tests

Run all tests in `tests/unit/`:
```bash
./zox tests/run_unit_tests.zo
```

Run a single test file:
```bash
./zox tests/unit/recursion.zo
```

Run lib unit tests:
```bash
./zox tests/run_libs_unit_tests.zo
```

### Test Organization

**Core tests** (`tests/unit/`):
- `append.zo` - List append operations
- `assert_module.zo` - Assertion functions
- `basic_types.zo` - Basic operations (lists, dicts, arithmetic)
- `benchmarks.zo` - Performance benchmarks
- `benchmarks_memory.zo` - Memory stress tests
- `cartesian.zo` - Cartesian product
- `collections_edge.zo` - Advanced collection operations
- `collections_nested_edges.zo` - Complex nested structures
- `control_flow.zo` - Conditionals, loops, break/continue/return
- `dict_helpers.zo` - Dict utility functions (has_key, get, setdefault)
- `framework_demo.zo` - Test framework demonstration
- `higher_order.zo` - Higher-order functions, closures, currying
- `recursion.zo` - Recursive and iterative functions
- `shadowing.zo` - Variable shadowing in loops
- `strings.zo` - String operations
- `while_let.zo` - Loop scoping and closures

**Library tests** (`tests/libs/unit/`):
- `json.zo` - JSON parsing and stringification
- `functional.zo` - Map, filter, reduce, etc.
- `collections.zo` - Range, zip, flatten, unique, chunk, count

### Available Assertions

```zox
assert_eq(actual, expected)      -# equality
assert_neq(a, b)                 -# inequality
assert_true(condition)           -# boolean true
assert_false(condition)          -# boolean false
assert_lt(a, b)                  -# less than
assert_gt(a, b)                  -# greater than
assert_lte(a, b)                 -# less than or equal
assert_gte(a, b)                 -# greater than or equal
assert_in(item, list)            -# membership
assert_not_in(item, list)        -# non-membership
assert_len(collection, n)        -# length
assert_contains(str, substr)     -# substring
assert_type(value, typename)     -# type check
assert_nil(value)                -# is nil
```

## Legacy Regression Tests

Legacy tests are in `tests/` and `tests/libs/`. Each test has:
- `.zo` file: source code
- `.expected` file: expected output

### Running Legacy Tests

Run all regression tests:
```bash
make test          -# core tests
make testlibs      -# library tests
```

The test runner compares actual output against `.expected` files.

## Writing Unit Tests

### Example: List operations

```zox
~> test { test, suite,
           assert_eq, assert_len,
           test_summary };

suite("List operations");

$ list_append() {
    let xs = {1, 2};
    xs << 3;
    assert_len(xs, 3);
    assert_eq(xs[2], 3)
}

$ list_concat() {
    let result = {1, 2} + {3, 4};
    assert_eq(result, {1, 2, 3, 4})
}

test("append", list_append);
test("concat", list_concat);

test_summary()
```

### Example: With imported modules

```zox
~> test { test, suite,
           assert_eq, assert_type,
           test_summary };

~> "./lib/json.so" { parse, stringify };

suite("JSON");

$ json_parse() {
    let arr = parse("[1, 2, 3]");
    assert_type(arr, "list");
    assert_eq(arr[0], 1)
}

test("parse", json_parse);

test_summary()
```

### Best Practices

1. **One assertion per test concern**: Each test should verify one logical aspect
2. **Descriptive test names**: Use snake_case that explains what's being tested
3. **Setup in function body**: Keep setup close to assertions for clarity
4. **No comments**: Well-named functions and assertions are self-documenting
5. **Group with suites**: Use `suite()` to organize related tests
6. **Test edge cases**: Empty collections, nil, out-of-range, etc.

## Build and Test Workflow

```bash
# Build everything
make full

# Run all unit tests
./zox tests/run_unit_tests.zo
./zox tests/run_libs_unit_tests.zo

# Run regression tests
make test
make testlibs

# Check performance baseline
make perf
```

## Test Coverage

- **Core**: 25 unit test files covering language features
- **Libraries**: 3 unit test files for json, functional, collections
- **Legacy**: 32 regression tests in `tests/`
- **Legacy libs**: 3 regression tests in `tests/libs/`

## Benchmarks

Performance benchmarks are in `tests/unit/benchmarks.zo` and `tests/unit/benchmarks_memory.zo`:

- Fibonacci (recursive)
- Bubble sort (200 items)
- Frequency count (300 items)
- Map/filter/reduce (400 items)
- String concatenation (1000 iterations)
- Cartesian product (30×30 items)
- Mutual recursion (200 calls)
- Large dataset stress test (5000+ items)

Run benchmarks:
```bash
./zox tests/unit/benchmarks.zo
./zox tests/unit/benchmarks_memory.zo
```

## Memory Testing

AddressSanitizer is available for memory safety testing:

```bash
make dev              # build with ASan
./zox file.zo         # ASan reports leaks on exit
```

All tests pass with zero memory leaks.
