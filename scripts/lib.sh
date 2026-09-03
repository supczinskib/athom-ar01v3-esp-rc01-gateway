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
    echo "ERROR: esphome/secrets.yaml is missing. Run scripts/02_configure.sh" >&2
    return 1
  fi
}

selected_device_number() {
  local root="$1"
  local settings="$root/esphome/climate.local.json"
  local receiver
  if [[ ! -f "$settings" ]]; then
    echo "ERROR: no receiver is selected. Run scripts/02_configure.sh DEVICE_NUMBER_01_TO_10" >&2
    return 1
  fi
  receiver="$(python3 - "$settings" <<'PY'
from pathlib import Path
import json
import sys

path = Path(sys.argv[1])
try:
    data = json.loads(path.read_text(encoding="utf-8"))
except (OSError, ValueError, TypeError) as error:
    raise SystemExit(f"ERROR: invalid {path.name}: {error}")
value = data.get("selected_receiver")
if not isinstance(value, str):
    raise SystemExit(
        f"ERROR: no receiver is selected in {path.name}; "
        "run scripts/02_configure.sh DEVICE_NUMBER_01_TO_10"
    )
print(value)
PY
  )" || return 1
  normalize_device_number "$receiver"
}

resolve_device_number() {
  local root="$1"
  if [[ -n "${AR01V3_DEVICE_OVERRIDE:-}" ]]; then
    normalize_device_number "$AR01V3_DEVICE_OVERRIDE"
  else
    selected_device_number "$root"
  fi
}

prepare_climate_args() {
  local root="$1"
  local receiver="$2"
  local settings="$root/esphome/climate.local.json"
  local values=()
  local output value

  if ! output="$(python3 - "$settings" "$receiver" <<'PY'
from pathlib import Path
import json
import sys

supported = {
    "ballu", "coolix", "daikin", "daikin_arc", "daikin_brc", "delonghi",
    "emmeti", "fujitsu_general", "fujitsu_vertical", "hitachi_ac344", "hitachi_ac424",
    "climate_ir_lg", "midea_ir", "mitsubishi", "noblex", "tcl112",
    "toshiba", "whirlpool", "whynter", "zhlt01",
}
device_names = {
    "ballu": "Ballu",
    "coolix": "Coolix",
    "daikin": "Daikin",
    "daikin_arc": "Daikin",
    "daikin_brc": "Daikin",
    "delonghi": "Delonghi",
    "emmeti": "Emmeti",
    "fujitsu_general": "Fujitsu",
    "fujitsu_vertical": "Fujitsu",
    "hitachi_ac344": "Hitachi",
    "hitachi_ac424": "Hitachi",
    "climate_ir_lg": "LG",
    "midea_ir": "Midea",
    "mitsubishi": "Mitsubishi",
    "noblex": "Noblex",
    "tcl112": "TCL",
    "toshiba": "Toshiba",
    "whirlpool": "Whirlpool",
    "whynter": "Whynter",
    "zhlt01": "ZH/LT-01",
}
profile_labels = {
    "ballu": "Ballu",
    "coolix": "Coolix",
    "daikin": "Daikin",
    "daikin_arc": "Daikin ARC",
    "daikin_brc": "Daikin BRC",
    "delonghi": "Delonghi",
    "emmeti": "Emmeti",
    "fujitsu_general": "Fujitsu General",
    "fujitsu_vertical": "Fujitsu General - vertical vane only",
    "hitachi_ac344": "Hitachi AC344",
    "hitachi_ac424": "Hitachi AC424",
    "climate_ir_lg": "LG",
    "midea_ir": "Midea",
    "mitsubishi": "Mitsubishi",
    "noblex": "Noblex",
    "tcl112": "TCL112",
    "toshiba": "Toshiba",
    "whirlpool": "Whirlpool",
    "whynter": "Whynter",
    "zhlt01": "ZH/LT-01",
}
path = Path(sys.argv[1])
receiver = sys.argv[2]
profile = "coolix"
if path.exists():
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        profile = data.get("receivers", {}).get(receiver, "coolix")
    except (OSError, ValueError, TypeError) as error:
        raise SystemExit(f"ERROR: invalid {path.name}: {error}")
if profile == "disabled":
    print("disabled")
    print("coolix")
    print("Coolix")
    print("Disabled")
elif profile == "fujitsu_vertical":
    print("fujitsu_vertical")
    print("fujitsu_vertical")
    print(device_names[profile])
    print(profile_labels[profile])
elif profile in supported:
    print("enabled")
    print(profile)
    print(device_names[profile])
    print(profile_labels[profile])
else:
    raise SystemExit(f"ERROR: unsupported climate profile for AR01V3 {receiver}: {profile}")
PY
  )"; then
    return 1
  fi
  while IFS= read -r value; do
    values+=("$value")
  done <<<"$output"
  if [[ ${#values[@]} -ne 4 ]]; then
    echo "ERROR: failed to load the climate profile for AR01V3 $receiver" >&2
    return 1
  fi
  ESPHOME_CLIMATE_ARGS=(
    -s climate_profile "${values[0]}"
    -s AC_Platform_name "${values[1]}"
    -s AC_Device_name "${values[2]}"
  )
  AR01V3_CLIMATE_PROFILE="${values[3]}"
}

print_selected_configuration() {
  local root="$1"
  local receiver="$2"
  prepare_climate_args "$root" "$receiver"
  printf 'Selected receiver: AR01V3 %s\n' "$receiver"
  printf 'Climate profile: %s\n' "$AR01V3_CLIMATE_PROFILE"
  printf 'Configuration: esphome/ar01v3-%s.yaml\n' "$receiver"
}

patch_esphome_api() {
  local esphome_bin="$1"
  local venv_python
  venv_python="$(dirname "$esphome_bin")/python"
  if [[ ! -x "$venv_python" ]]; then
    echo "ERROR: cannot locate the Python interpreter belonging to $esphome_bin" >&2
    return 1
  fi
  "$venv_python" "$(project_root)/scripts/patch_esphome_api.py"
  "$venv_python" "$(project_root)/scripts/patch_esphome_ble_tracker.py"
}
