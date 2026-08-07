#!/usr/bin/env bash
set -u
set -o pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib.sh
source "$ROOT_DIR/scripts/lib.sh"

# Fail immediately on known project regressions before invoking ESPHome ten times.
"$ROOT_DIR/scripts/00_self_test.sh" || exit 1

ESPHOME_BIN="$(find_esphome)" || exit 1
require_secrets "$ROOT_DIR" || exit 1

cd "$ROOT_DIR/esphome" || exit 1
PASSED=0
FAILED=0

for n in $(seq -w 1 10); do
  config="ar01v3-$n.yaml"
  output="$(mktemp)"
  echo "=== Validating AR01V3 $n ==="

  if "$ESPHOME_BIN" config "$config" >"$output" 2>&1; then
    grep -E '^(WARNING|ERROR)' "$output" || true
    echo "OK: $config"
    PASSED=$((PASSED + 1))
  else
    cat "$output"
    echo "ERROR: $config"
    FAILED=$((FAILED + 1))
  fi
  rm -f "$output"
done

echo
echo "=== ESPHome validation summary ==="
echo "Passed: $PASSED"
echo "Failed: $FAILED"

if (( FAILED > 0 )); then
  exit 1
fi

echo 'All ESPHome configurations passed validation.'
