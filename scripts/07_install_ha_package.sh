#!/usr/bin/env bash
set -euo pipefail
HA_CONFIG=${1:-/var/lib/homeassistant}
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RAW="$ROOT_DIR/home-assistant/esp_rc01_10x10_package.yaml"
WRAPPED="$ROOT_DIR/home-assistant/esp_rc01_10x10_package_merge_named.yaml"
DST_DIR="$HA_CONFIG/packages"
DST="$DST_DIR/esp_rc01_10x10_package.yaml"
CONFIG="$HA_CONFIG/configuration.yaml"

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "Run as root: sudo bash $0 [HOME_ASSISTANT_CONFIG_DIRECTORY]"
  exit 1
fi
[[ -f "$CONFIG" ]] || { echo "$CONFIG was not found"; exit 1; }
mkdir -p "$DST_DIR"
BACKUP="$CONFIG.backup-espnow-$(date +%Y%m%d-%H%M%S)"
cp -a "$CONFIG" "$BACKUP"

echo "configuration.yaml backup: $BACKUP"
if grep -Eq '^[[:space:]]+packages:[[:space:]]*!include_dir_merge_named[[:space:]]+packages[[:space:]]*$' "$CONFIG"; then
  install -m 0644 "$WRAPPED" "$DST"
  echo 'Detected !include_dir_merge_named; installed the ESP-RC01 package.'
elif grep -Eq '^[[:space:]]+packages:[[:space:]]*!include_dir_named[[:space:]]+packages[[:space:]]*$' "$CONFIG"; then
  install -m 0644 "$RAW" "$DST"
  echo 'Detected !include_dir_named; installed the ESP-RC01 package.'
else
  python3 - "$CONFIG" <<'PY2'
from pathlib import Path
import sys
p=Path(sys.argv[1])
lines=p.read_text().splitlines(True)
for line in lines:
    if line.lstrip().startswith('packages:'):
        raise SystemExit('configuration.yaml contains a different packages directive. The file was not changed automatically.')
idx=None
other_homeassistant=[]
for i,line in enumerate(lines):
    stripped=line.rstrip('\r\n')
    if stripped == 'homeassistant:':
        idx=i
        break
    if line.startswith('homeassistant:'):
        other_homeassistant.append((i + 1, stripped))
if idx is None and other_homeassistant:
    line_no, content = other_homeassistant[0]
    raise SystemExit(
        f'Detected a non-standard homeassistant section on line {line_no}: {content!r}. '
        'configuration.yaml was not changed automatically; follow the manual README instructions.'
    )
if idx is None:
    if lines and not lines[-1].endswith('\n'):
        lines[-1]+='\n'
    lines += ['\nhomeassistant:\n', '  packages: !include_dir_named packages\n']
else:
    lines.insert(idx+1, '  packages: !include_dir_named packages\n')
p.write_text(''.join(lines))
PY2
  install -m 0644 "$RAW" "$DST"
  echo 'Added homeassistant/packages: !include_dir_named packages.'
fi
chown --reference="$CONFIG" "$DST" 2>/dev/null || true
chmod 0644 "$DST"
echo "Home Assistant package: $DST"
