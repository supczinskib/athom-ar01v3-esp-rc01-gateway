#!/usr/bin/env bash
set -euo pipefail
[[ $# -le 1 ]] || { echo "Usage: $0 [IP_OR_HOSTNAME]"; exit 1; }
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib.sh
source "$ROOT_DIR/scripts/lib.sh"
N="$(resolve_device_number "$ROOT_DIR")"
TARGET="${1:-ar01v3-espnow-$N.local}"
ESPHOME_BIN="$(find_esphome)"
require_secrets "$ROOT_DIR"
print_selected_configuration "$ROOT_DIR" "$N"
echo "Log target: $TARGET"
cd "$ROOT_DIR/esphome"
"$ESPHOME_BIN" "${ESPHOME_CLIMATE_ARGS[@]}" logs "ar01v3-$N.yaml" --device "$TARGET"
