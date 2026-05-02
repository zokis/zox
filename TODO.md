# TODO

## Performance: lists and dicts

- [x] Replace repeated `list = list + {x}` patterns in hot paths with `list << x`.
- [x] Add dict helpers like `has_key(dict, key)` and `get(dict, key)`.
- [x] Add `setdefault(dict, key, default)`-style support only if the arity
  model and ownership rules stay simple.
- [x] Avoid `find(keys(dict), key)` in loops when direct lookup is available.
- [x] Review `runtime_value_to_string()` ownership for dict keys to remove extra alloc/free churn.
- [x] Reduce avoidable `retain()` / `release()` traffic in list element-wise
  operators by transferring ownership of freshly built results instead of
  retaining each element twice.
- [x] Add internal `dict_find_entry()` / `dict_get_val()` helpers to centralize lookup logic.
- [x] Raise list minimum initial capacity from 1 to 8; the current `2x+1` growth from 1 causes ~9
  reallocations before reaching 400 elements (the bench make_list size).
- [x] Raise dict minimum initial capacity from 1 to 8 for empty `[]` literals; capacity-1 triggers
  an immediate resize on the very first insert.
- [x] Implement append-in-place for `list + rhs` when the left operand has `refcount == 1`: avoids
  allocating a new items[] array on every iteration of accumulator loops.
- [x] Implement open addressing (linear probing) for DictVal instead of chained entries: eliminates
  the 150k+ separate Entry allocs seen in the bench and improves lookup cache locality.
- [x] Fix ref-counting self-assignment bug in loop body evaluation that corrupted results in nested loops.

## Benchmarking and validation

- Keep `scripts/compare_arena.sh` as the standard local benchmark for arena tuning.
- Compare `no-arena` against the smallest arena that does not overflow.
- Use `--alloc-stats` to find `arena used=... / total=...` before choosing a size.
- Validate changes with repeated runs and RSS checks, not a single timing sample.

## Arena sizing guidance

- Document that `--arena` is a tuning knob, not a guaranteed speedup.
- Warn that a too-small arena can be slower than no arena because overflow falls back to `malloc`.
- Prefer a size with headroom over an almost-full arena.
- In arena mode, heap-overflow objects are freed immediately (not pooled), so an undersized arena
  produces more malloc/free traffic than no arena at all. Example: bench_memory.zo with 4MB arena
  had 251k heap allocs for NumberVal vs 23k without arena, causing a ~3s spike.
- The correct arena size is the first power-of-two above `arena used=` reported by `--alloc-stats`.
  For bench_memory.zo that is 8MB (used ≈ 6.2MB with 16MB total); 16MB gives the same perf with
  extra headroom.

## Memory leak fixes (ASan-detected, bench.zo baseline)

**Status**: [x] FIXED. All types show `alloc == freed` in `--alloc-stats`. ASan reports zero leaks.

### Root cause (resolved)

The ~252KB of indirect leaks were not caused by broken refcount logic in list/dict operations.
They were caused by a **closure reference cycle** in `free_environment`:

- `make_adder` defines inner function `adder` that captures `make_adder`'s func_env.
- func_env stores `adder` (retain) AND `adder.env = func_env` (retain_env).
- Cycle: `func_env["adder"] → adder_func → adder_func.env → func_env`.
- `make_adder_func_env.parent = global_env` was never released, keeping `global_env` ref_count > 0.
- `destroy_environment(global_env)` was never called → all global variables leaked.

### Fix applied (`env.c: free_environment`)

When `env->ref_count > 1` at `free_environment` time, scan entries for FunctionVals whose
`.env == env` (self-referential closure). Remove the env's hold on those functions.
The function's own `.env` pointer is kept intact so it can still look up captured variables.
This breaks the cycle without affecting closure correctness.

## Test coverage

- Keep the higher-order function tests for mutual recursion, closures, factories, and currying.
- Keep the nested collection tests for lists of dicts, dicts of lists, out-of-range slices, missing dict keys, and element-wise operators.
- Add regression tests for any dict helper API that gets introduced.
