#!/usr/bin/env bash
# scripts/perf_test.sh — performance regression test
#
# Usage:
#   ./scripts/perf_test.sh            # compare against saved baseline
#   ./scripts/perf_test.sh --update   # overwrite baseline with current measurements
#
# The baseline file is local-only (.gitignore'd) so each machine compares
# against itself, not against another developer's hardware.

set -euo pipefail

BASELINE="tests/perf_baseline.txt"
BENCH="tests/bench.zo"
RUNS=10
THRESHOLD_PCT=10   # fail if median is >10% slower than baseline

cd "$(dirname "$0")/.."

if [[ ! -x ./zox ]]; then
  make all -s
fi

echo "bench:     $BENCH"
echo "runs:      $RUNS"
echo "threshold: ${THRESHOLD_PCT}%"
echo

# Collect wall-clock times in milliseconds
times=()
for i in $(seq 1 $RUNS); do
  start=$(date +%s%N)
  ./zox "$BENCH" > /dev/null
  end=$(date +%s%N)
  ms=$(( (end - start) / 1000000 ))
  times+=($ms)
  printf "  run %d: %dms\n" "$i" "$ms"
done

# Sort numerically and pick the middle value (median)
IFS=$'\n' sorted=($(sort -n <<< "${times[*]}")); unset IFS
median=${sorted[$((RUNS / 2))]}
echo
echo "  median: ${median}ms"

# --update: save new baseline and exit
if [[ "${1:-}" == "--update" ]]; then
  echo "$median" > "$BASELINE"
  echo "Baseline updated → ${median}ms"
  exit 0
fi

# Compare against saved baseline
if [[ ! -f "$BASELINE" ]]; then
  echo
  echo "No baseline found at $BASELINE."
  echo "Run 'make perf-update' to create one."
  exit 1
fi

baseline=$(cat "$BASELINE")
limit=$(( baseline + baseline * THRESHOLD_PCT / 100 ))

echo "  baseline: ${baseline}ms"
echo "  limit:    ${limit}ms  (baseline + ${THRESHOLD_PCT}%)"
echo

if (( median > limit )); then
  echo "FAIL: ${median}ms exceeds limit of ${limit}ms (regression of $(( (median - baseline) * 100 / baseline ))%)"
  exit 1
else
  delta=$(( (median - baseline) * 100 / baseline ))
  if (( delta >= 0 )); then
    echo "PASS: ${median}ms  (+${delta}% vs baseline)"
  else
    echo "PASS: ${median}ms  (${delta}% vs baseline, improvement)"
  fi
  exit 0
fi
