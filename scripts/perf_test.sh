#!/usr/bin/env bash
# scripts/perf_test.sh — performance regression test
#
# Usage:
#   ./scripts/perf_test.sh            # compare against saved baseline
#   ./scripts/perf_test.sh --update   # overwrite baseline with current measurements

set -euo pipefail

BASELINE="tests/benchmarks/perf_baseline.txt"
BENCH="tests/benchmarks/bench.zo"
RUNS=10
THRESHOLD_PCT=10   # fail if median is >10% slower than baseline
ARENA_SIZE="16MB"

cd "$(dirname "$0")/.."

if [[ ! -x ./zox ]]; then
  make all -s
fi

echo "bench:      $BENCH"
echo "runs:       $RUNS"
echo "threshold:  ${THRESHOLD_PCT}%"
echo "arena size: $ARENA_SIZE"
echo

# Helper to run a benchmark and return the median time
measure_median() {
  local label="$1"
  local flags="$2"
  local times=()

  echo "Measuring $label..." >&2
  for i in $(seq 1 $RUNS); do
    start=$(date +%s%N)
    ./zox $flags "$BENCH" > /dev/null
    end=$(date +%s%N)
    ms=$(( (end - start) / 1000000 ))
    times+=($ms)
    printf "  run %d: %3dms\n" "$i" "$ms" >&2
  done

  IFS=$'\n' sorted=($(sort -n <<< "${times[*]}")); unset IFS
  local median=${sorted[$((RUNS / 2))]}
  echo "  median: ${median}ms" >&2
  echo >&2
  echo "$median"
}

# Run both modes
median_std=$(measure_median "Standard" "")
median_arena=$(measure_median "Arena" "--arena=$ARENA_SIZE")

# --update: save new baseline and exit
if [[ "${1:-}" == "--update" ]]; then
  echo "$median_std" > "$BASELINE"
  echo "$median_arena" >> "$BASELINE"
  echo "Baseline updated:"
  echo "  Standard: ${median_std}ms"
  echo "  Arena:    ${median_arena}ms"
  exit 0
fi

# Compare against saved baseline
if [[ ! -f "$BASELINE" ]]; then
  echo "No baseline found at $BASELINE."
  echo "Run 'make perf-update' to create one."
  exit 1
fi

# Read baseline (handle legacy format with only 1 line)
mapfile -t baselines < "$BASELINE"
base_std=${baselines[0]:-0}
base_arena=${baselines[1]:-0}

check_perf() {
  local label="$1"
  local current="$2"
  local baseline="$3"

  if (( baseline == 0 )); then
    echo "  $label: ${current}ms (no baseline)"
    return 0
  fi

  local limit=$(( baseline + baseline * THRESHOLD_PCT / 100 ))
  local delta=$(( (current - baseline) * 100 / baseline ))

  if (( current > limit )); then
    echo "  $label: FAIL: ${current}ms exceeds limit of ${limit}ms (regression of ${delta}%)"
    return 1
  else
    if (( delta >= 0 )); then
      echo "  $label: PASS: ${current}ms  (+${delta}% vs ${baseline}ms baseline)"
    else
      echo "  $label: PASS: ${current}ms  (${delta}% vs ${baseline}ms baseline, improvement)"
    fi
    return 0
  fi
}

echo "Results:"
exit_code=0
check_perf "Standard" "$median_std" "$base_std" || exit_code=1
check_perf "Arena   " "$median_arena" "$base_arena" || exit_code=1

exit $exit_code
