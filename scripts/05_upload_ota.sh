#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "Usage: $0 DEVICE_NUMBER_01_TO_10 IP_OR_HOSTNAME"; exit 1; }
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib.sh
source "$ROOT_DIR/scripts/lib.sh"
N="$(normalize_device_number "$1")"
TARGET="$2"
ESPHOME_BIN="$(find_esphome)"
require_secrets "$ROOT_DIR"
cd "$ROOT_DIR/esphome"
"$ESPHOME_BIN" clean "ar01v3-$N.yaml"
"$ESPHOME_BIN" compile "ar01v3-$N.yaml"
python3 "$ROOT_DIR/scripts/verify_embedded_main_ui.py" \
  "$ROOT_DIR/esphome/ar01v3_web_v3.js" \
  "$ROOT_DIR/esphome/.esphome/build/ar01v3-espnow-$N/src/main.cpp"
"$ESPHOME_BIN" upload "ar01v3-$N.yaml" --device "$TARGET"
