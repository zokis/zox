#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

TEST_CASES=(
  "examples/compile_test.zo"
  "examples/variables_test.zo"
  "examples/if_test.zo"
  "examples/loops_test.zo"
  "examples/functions_test.zo"
  "examples/collections_test.zo"
  "examples/compiler_benchmark.zo"
)

TMP_DIR="$(mktemp -d /tmp/zoxc-test-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

pass_count=0

for test_file in "${TEST_CASES[@]}"; do
  base_name="$(basename "$test_file" .zo)"
  expected_out="$TMP_DIR/${base_name}.expected"
  actual_out="$TMP_DIR/${base_name}.actual"
  compiled_bin="$TMP_DIR/${base_name}.bin"

  ./zox "$test_file" > "$expected_out"
  ./zoxc "$test_file" -o "$compiled_bin" > /dev/null
  "$compiled_bin" > "$actual_out"

  if diff -u "$expected_out" "$actual_out" > "$TMP_DIR/${base_name}.diff"; then
    printf 'PASS %s\n' "$test_file"
    pass_count=$((pass_count + 1))
  else
    printf 'FAIL %s\n' "$test_file"
    cat "$TMP_DIR/${base_name}.diff"
    exit 1
  fi
done

printf '\nCompiler tests: %d passed / 0 failed\n' "$pass_count"
