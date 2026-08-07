#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo "Usage: $0 DEVICE_NUMBER_01_TO_10"; exit 1; }
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib.sh
source "$ROOT_DIR/scripts/lib.sh"
N="$(normalize_device_number "$1")"
ESPHOME_BIN="$(find_esphome)"
require_secrets "$ROOT_DIR"
cd "$ROOT_DIR/esphome"
"$ESPHOME_BIN" compile "ar01v3-$N.yaml"
echo
echo 'Build artifacts:'
find .esphome/build -type f \( -name 'firmware.factory.bin' -o -name 'firmware.bin' -o -name 'firmware.ota.bin' \) -path "*ar01v3-espnow-$N*" -print 2>/dev/null || true
