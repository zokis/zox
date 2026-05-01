#!/usr/bin/env bash
set -euo pipefail

runs="${RUNS:-10}"
arena="${ARENA:-4MB}"
bench="${1:-tests/bench.zo}"

if [[ ! -x ./zox ]]; then
  make
fi

if [[ ! -f "$bench" ]]; then
  echo "benchmark not found: $bench" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

run_case() {
  local name="$1"
  shift
  local out="$tmpdir/$name.tsv"

  for i in $(seq "$runs"); do
    /usr/bin/time -f "%e %M" -o "$out" -a "$@" >/dev/null
  done

  awk -v name="$name" '
    NR == 1 || $1 < min_time { min_time = $1 }
    $1 > max_time { max_time = $1 }
    $2 > max_rss { max_rss = $2 }
    { total_time += $1; total_rss += $2 }
    END {
      printf "%-10s avg=%0.4fs min=%0.4fs max=%0.4fs avg_rss=%0.0fKB max_rss=%dKB\n",
             name, total_time / NR, min_time, max_time, total_rss / NR, max_rss
    }
  ' "$out"
}

echo "benchmark: $bench"
echo "runs:      $runs"
echo "arena:     $arena"
echo

run_case "no-arena" ./zox "$bench"
run_case "arena" ./zox "--arena=$arena" "$bench"

echo
echo "allocator stats: no arena"
./zox --alloc-stats "$bench" >/dev/null

echo
echo "allocator stats: arena"
./zox "--arena=$arena" --alloc-stats "$bench" >/dev/null
