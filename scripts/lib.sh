#!/usr/bin/env bash

project_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
}

find_esphome() {
  local candidate="${ESPHOME:-/opt/esphome-10x10/bin/esphome}"
  if [[ -x "$candidate" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi
  candidate="$(command -v esphome 2>/dev/null || true)"
  if [[ -n "$candidate" && -x "$candidate" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi
  echo "ERROR: ESPHome was not found. Run scripts/01_install_esphome.sh" >&2
  return 1
}

normalize_device_number() {
  local raw="${1:-}"
  if [[ ! "$raw" =~ ^([1-9]|10|0[1-9])$ ]]; then
    echo "ERROR: the device number must be in the range 01..10" >&2
    return 1
  fi
  printf '%02d\n' "$((10#$raw))"
}

require_secrets() {
  local root="$1"
  if [[ ! -f "$root/esphome/secrets.yaml" ]]; then
    echo "ERROR: esphome/secrets.yaml is missing. Run scripts/02_configure_secrets.sh" >&2
    return 1
  fi
}
