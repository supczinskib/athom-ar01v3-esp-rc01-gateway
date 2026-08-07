#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
for n in $(seq -w 1 10); do
  "$ROOT_DIR/scripts/04_compile_one.sh" "$n"
done
