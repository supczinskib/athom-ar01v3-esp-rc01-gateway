#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SECRETS="$ROOT_DIR/esphome/secrets.yaml"
CLIMATE_SETTINGS="$ROOT_DIR/esphome/climate.local.json"
# shellcheck source=lib.sh
source "$ROOT_DIR/scripts/lib.sh"

[[ $# -eq 1 ]] || {
  echo "Usage: $0 DEVICE_NUMBER_01_TO_10" >&2
  exit 1
}
SELECTED_RECEIVER="$(normalize_device_number "$1")"

CLIMATE_PROFILE_IDS=(
  disabled ballu coolix daikin daikin_arc daikin_brc delonghi emmeti
  fujitsu_general fujitsu_vertical hitachi_ac344 hitachi_ac424 climate_ir_lg
  midea_ir mitsubishi noblex tcl112 toshiba whirlpool whynter zhlt01
)

CLIMATE_PROFILE_LABELS=(
  "Disabled" "Ballu" "Coolix" "Daikin" "Daikin ARC" "Daikin BRC"
  "Delonghi" "Emmeti" "Fujitsu General"
  "Fujitsu General - vertical vane only" "Hitachi AC344" "Hitachi AC424"
  "LG" "Midea" "Mitsubishi" "Noblex" "TCL112" "Toshiba"
  "Whirlpool" "Whynter" "ZH/LT-01"
)

read_existing_secret() {
  local key="$1"
  python3 - "$SECRETS" "$key" <<'PY'
from pathlib import Path
import json
import sys

path = Path(sys.argv[1])
key = sys.argv[2]
if not path.exists():
    raise SystemExit

for raw in path.read_text(encoding="utf-8").splitlines():
    line = raw.strip()
    if not line or line.startswith("#") or ":" not in line:
        continue
    candidate, value = line.split(":", 1)
    if candidate.strip() != key:
        continue
    value = value.strip()
    try:
        print(json.loads(value))
    except Exception:
        print(value.strip('"\''))
    break
PY
}

prompt_visible() {
  local label="$1"
  local current="$2"
  local value
  if [[ -n "$current" ]]; then
    printf '%s [%s]: ' "$label" "$current" >&2
    read -r value
    printf '%s' "${value:-$current}"
  else
    printf '%s: ' "$label" >&2
    read -r value
    if [[ -z "$value" ]]; then
      printf '%s cannot be empty.\n' "$label" >&2
      return 1
    fi
    printf '%s' "$value"
  fi
}

prompt_hidden() {
  local label="$1"
  local current="$2"
  local generated_default="${3:-}"
  local value
  if [[ -n "$current" ]]; then
    printf '%s [********; Enter keeps the current value]: ' "$label" >&2
    read -r -s value
    echo >&2
    printf '%s' "${value:-$current}"
  elif [[ -n "$generated_default" ]]; then
    printf '%s [Enter generates a secure value]: ' "$label" >&2
    read -r -s value
    echo >&2
    printf '%s' "${value:-$generated_default}"
  else
    printf '%s: ' "$label" >&2
    read -r -s value
    echo >&2
    if [[ -z "$value" ]]; then
      printf '%s cannot be empty.\n' "$label" >&2
      return 1
    fi
    printf '%s' "$value"
  fi
}

confirm_changed_password() {
  local label="$1"
  local current="$2"
  local value="$3"
  local repeated
  if [[ -n "$current" && "$value" == "$current" ]]; then
    return 0
  fi
  printf 'Repeat %s: ' "$label"
  read -r -s repeated
  echo
  if [[ "$value" != "$repeated" ]]; then
    printf '%s values do not match.\n' "$label" >&2
    return 1
  fi
}

read_climate_profile() {
  local receiver="$1"
  python3 - "$CLIMATE_SETTINGS" "$receiver" <<'PY'
from pathlib import Path
import json
import sys

path = Path(sys.argv[1])
receiver = sys.argv[2]
profile = "coolix"
if path.exists():
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        value = data.get("receivers", {}).get(receiver, "coolix")
    except (OSError, ValueError, TypeError) as error:
        raise SystemExit(f"ERROR: invalid {path.name}: {error}")
    if not isinstance(value, str):
        raise SystemExit(f"ERROR: invalid climate profile for AR01V3 {receiver}")
    profile = value
print(profile)
PY
}

choose_climate_profile() {
  local current="$1"
  local selection index
  echo
  printf 'Current profile: %s\n' "$(climate_profile_label "$current")"
  print_climate_profile_menu
  printf 'Selection [Enter keeps current profile]: '
  read -r selection
  if [[ -z "$selection" ]]; then
    CHOSEN_CLIMATE_PROFILE="$current"
    return 0
  fi
  if [[ ! "$selection" =~ ^[0-9]+$ ]]; then
    echo "Invalid selection: $selection" >&2
    return 1
  fi
  index=$((10#$selection - 1))
  if (( index < 0 || index >= ${#CLIMATE_PROFILE_IDS[@]} )); then
    echo "Invalid selection: $selection" >&2
    return 1
  fi
  CHOSEN_CLIMATE_PROFILE="${CLIMATE_PROFILE_IDS[$index]}"
}

climate_profile_label() {
  local profile="$1"
  local index
  for ((index = 0; index < ${#CLIMATE_PROFILE_IDS[@]}; index++)); do
    if [[ "${CLIMATE_PROFILE_IDS[$index]}" == "$profile" ]]; then
      printf '%s' "${CLIMATE_PROFILE_LABELS[$index]}"
      return 0
    fi
  done
  printf '%s' "$profile"
}

print_climate_profile_menu() {
  local columns=3
  local total=${#CLIMATE_PROFILE_IDS[@]}
  local rows=$(((total + columns - 1) / columns))
  local row column index cell width
  local widths=(0 0 0)

  for ((column = 0; column < columns; column++)); do
    for ((row = 0; row < rows; row++)); do
      index=$((row + column * rows))
      (( index >= total )) && continue
      cell="$((index + 1)). ${CLIMATE_PROFILE_LABELS[$index]}"
      (( ${#cell} > widths[column] )) && widths[column]=${#cell}
    done
  done

  for ((row = 0; row < rows; row++)); do
    printf '  '
    for ((column = 0; column < columns; column++)); do
      index=$((row + column * rows))
      (( index >= total )) && continue
      cell="$((index + 1)). ${CLIMATE_PROFILE_LABELS[$index]}"
      width=$((widths[column] + 3))
      printf "%-${width}s" "$cell"
    done
    printf '\n'
  done
}

write_climate_profile() {
  local receiver="$1"
  local profile="$2"
  python3 - "$CLIMATE_SETTINGS" "$receiver" "$profile" <<'PY'
from pathlib import Path
import json
import os
import sys
import tempfile

path = Path(sys.argv[1])
receiver = sys.argv[2]
profile = sys.argv[3]
data = {}
if path.exists():
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError) as error:
        raise SystemExit(f"ERROR: invalid {path.name}: {error}")
receivers = data.get("receivers", {})
if not isinstance(receivers, dict):
    raise SystemExit(f"ERROR: invalid receivers map in {path.name}")
receivers[receiver] = profile
data = {
    "version": 1,
    "selected_receiver": receiver,
    "receivers": receivers,
}
path.parent.mkdir(parents=True, exist_ok=True)
fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
try:
    with os.fdopen(fd, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")
        handle.flush()
        os.fsync(handle.fileno())
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)
finally:
    if os.path.exists(temporary):
        os.unlink(temporary)
PY
}

echo '=== Wi-Fi and access configuration ==='
OLD_WIFI_SSID="$(read_existing_secret wifi_ssid)"
OLD_WIFI_PASSWORD="$(read_existing_secret wifi_password)"
OLD_OTA_PASSWORD="$(read_existing_secret ota_password)"
OLD_FALLBACK_PASSWORD="$(read_existing_secret fallback_ap_password)"
OLD_WEB_USERNAME="$(read_existing_secret web_server_username)"
OLD_WEB_PASSWORD="$(read_existing_secret web_server_password)"

WIFI_SSID="$(prompt_visible 'Wi-Fi SSID' "$OLD_WIFI_SSID")"
WIFI_PASSWORD="$(prompt_hidden 'Wi-Fi password' "$OLD_WIFI_PASSWORD")"
OTA_DEFAULT="$(openssl rand -hex 16)"
OTA_PASSWORD="$(prompt_hidden 'OTA password' "$OLD_OTA_PASSWORD" "$OTA_DEFAULT")"
WEB_USERNAME="$(prompt_visible 'Web interface username' "${OLD_WEB_USERNAME:-admin}")"

while true; do
  WEB_PASSWORD="$(prompt_hidden 'Web interface password' "$OLD_WEB_PASSWORD")"
  if (( ${#WEB_PASSWORD} < 12 || ${#WEB_PASSWORD} > 63 )); then
    echo 'The web interface password must contain 12-63 characters.' >&2
    continue
  fi
  if confirm_changed_password 'web interface password' "$OLD_WEB_PASSWORD" "$WEB_PASSWORD"; then
    break
  fi
done

while true; do
  if [[ -n "$OLD_FALLBACK_PASSWORD" ]]; then
    printf 'Fallback AP password [********; Enter keeps the current value; =web uses the web password]: '
  else
    printf 'Fallback AP password [Enter uses the web password]: '
  fi
  read -r -s FALLBACK_INPUT
  echo
  if [[ -z "$FALLBACK_INPUT" ]]; then
    FALLBACK_PASSWORD="${OLD_FALLBACK_PASSWORD:-$WEB_PASSWORD}"
  elif [[ "$FALLBACK_INPUT" == "=web" ]]; then
    FALLBACK_PASSWORD="$WEB_PASSWORD"
  else
    FALLBACK_PASSWORD="$FALLBACK_INPUT"
  fi
  if (( ${#FALLBACK_PASSWORD} < 8 || ${#FALLBACK_PASSWORD} > 63 )); then
    echo 'The fallback AP password must contain 8-63 characters.' >&2
    continue
  fi
  if [[ -n "$OLD_FALLBACK_PASSWORD" && "$FALLBACK_PASSWORD" == "$OLD_FALLBACK_PASSWORD" ]] \
      || [[ "$FALLBACK_INPUT" == "=web" ]] \
      || [[ -z "$OLD_FALLBACK_PASSWORD" && -z "$FALLBACK_INPUT" ]]; then
    break
  fi
  if confirm_changed_password 'fallback AP password' "$OLD_FALLBACK_PASSWORD" "$FALLBACK_PASSWORD"; then
    break
  fi
done

python3 - "$SECRETS" \
  "$WIFI_SSID" "$WIFI_PASSWORD" "$OTA_PASSWORD" "$FALLBACK_PASSWORD" \
  "$WEB_USERNAME" "$WEB_PASSWORD" <<'PY'
from pathlib import Path
import json
import os
import sys
import tempfile

path = Path(sys.argv[1])
updates = {
    "wifi_ssid": sys.argv[2],
    "wifi_password": sys.argv[3],
    "ota_password": sys.argv[4],
    "fallback_ap_password": sys.argv[5],
    "web_server_username": sys.argv[6],
    "web_server_password": sys.argv[7],
}
lines = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
result = []
written = set()
for raw in lines:
    stripped = raw.strip()
    if stripped and not stripped.startswith("#") and ":" in raw:
        key = raw.split(":", 1)[0].strip()
        if key in updates:
            if key not in written:
                result.append(f"{key}: {json.dumps(updates[key], ensure_ascii=False)}")
                written.add(key)
            continue
    result.append(raw)
for key, value in updates.items():
    if key not in written:
        result.append(f"{key}: {json.dumps(value, ensure_ascii=False)}")
path.parent.mkdir(parents=True, exist_ok=True)
fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
try:
    with os.fdopen(fd, "w", encoding="utf-8") as handle:
        handle.write("\n".join(result).rstrip() + "\n")
        handle.flush()
        os.fsync(handle.fileno())
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)
finally:
    if os.path.exists(temporary):
        os.unlink(temporary)
PY

echo
echo '=== Native IR climate configuration ==='
CURRENT_CLIMATE_PROFILE="$(read_climate_profile "$SELECTED_RECEIVER")"
if [[ "$CURRENT_CLIMATE_PROFILE" != "disabled" ]] \
    && ! is_supported_climate_platform "$CURRENT_CLIMATE_PROFILE"; then
  echo "Ignoring unsupported saved profile for AR01V3 $SELECTED_RECEIVER: $CURRENT_CLIMATE_PROFILE" >&2
  CURRENT_CLIMATE_PROFILE="coolix"
fi
printf 'Selected receiver: AR01V3 %s\n' "$SELECTED_RECEIVER"
choose_climate_profile "$CURRENT_CLIMATE_PROFILE"
write_climate_profile "$SELECTED_RECEIVER" "$CHOSEN_CLIMATE_PROFILE"

unset WIFI_PASSWORD OLD_WIFI_PASSWORD OTA_PASSWORD OLD_OTA_PASSWORD OTA_DEFAULT
unset WEB_PASSWORD OLD_WEB_PASSWORD FALLBACK_PASSWORD OLD_FALLBACK_PASSWORD FALLBACK_INPUT

echo
echo "Created or updated: $SECRETS"
echo "Selected receiver: AR01V3 $SELECTED_RECEIVER"
echo "Climate profile: $CHOSEN_CLIMATE_PROFILE"
echo "Local build configuration: $CLIMATE_SETTINGS"
