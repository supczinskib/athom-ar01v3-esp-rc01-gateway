#!/usr/bin/env bash
set -euo pipefail
[[ $# -le 1 ]] || { echo "Usage: $0 [IP_OR_HOSTNAME]"; exit 1; }
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib.sh
source "$ROOT_DIR/scripts/lib.sh"
N="$(resolve_device_number "$ROOT_DIR")"
TARGET="${1:-ar01v3-espnow-$N.local}"
ESPHOME_BIN="$(find_esphome)"
patch_esphome_api "$ESPHOME_BIN"
require_secrets "$ROOT_DIR"
print_selected_configuration "$ROOT_DIR" "$N"
echo "OTA target: $TARGET"
cd "$ROOT_DIR/esphome"
"$ESPHOME_BIN" "${ESPHOME_CLIMATE_ARGS[@]}" clean "ar01v3-$N.yaml"
"$ESPHOME_BIN" "${ESPHOME_CLIMATE_ARGS[@]}" compile "ar01v3-$N.yaml"
python3 "$ROOT_DIR/scripts/verify_embedded_main_ui.py" \
  "$ROOT_DIR/esphome/ar01v3_web_v3.js" \
  "$ROOT_DIR/esphome/.esphome/build/ar01v3-espnow-$N/src/main.cpp"
"$ESPHOME_BIN" "${ESPHOME_CLIMATE_ARGS[@]}" upload "ar01v3-$N.yaml" --device "$TARGET"
