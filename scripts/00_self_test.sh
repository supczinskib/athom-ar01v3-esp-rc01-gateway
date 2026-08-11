#!/usr/bin/env bash
set -euo pipefail

PUBLICATION_MODE=false
case "${1:-}" in
  "") ;;
  --publication) PUBLICATION_MODE=true ;;
  *) echo "Usage: $0 [--publication]" >&2; exit 2 ;;
esac

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BASE="$ROOT_DIR/esphome/ar01v3-espnow-10x10-base.yaml"
FAIL=0
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

ok() { printf 'OK: %s\n' "$1"; }
info() { printf 'INFO: %s\n' "$1"; }
fail() { printf 'ERROR: %s\n' "$1" >&2; FAIL=$((FAIL + 1)); }

if [[ "$PUBLICATION_MODE" == true ]]; then
  printf '%s\n' '=== AR01V3 ESP-RC01 Gateway publication self-test ==='
else
  printf '%s\n' '=== AR01V3 ESP-RC01 Gateway self-test ==='
fi

# Check every shell script before running deeper tests.
for script in "$ROOT_DIR"/scripts/*.sh; do
  if bash -n "$script"; then
    ok "shell syntax: $(basename "$script")"
  else
    fail "invalid shell syntax: $(basename "$script")"
  fi
done

# Verify the ten receiver entry points.
[[ -f "$BASE" ]] || fail 'base ESPHome configuration is missing'
for n in $(seq -w 1 10); do
  cfg="$ROOT_DIR/esphome/ar01v3-$n.yaml"
  if [[ ! -f "$cfg" ]]; then
    fail "missing esphome/ar01v3-$n.yaml"
    continue
  fi
  grep -Fq 'ar01v3_espnow_10x10: !include ar01v3-espnow-10x10-base.yaml' "$cfg" \
    || fail "invalid base include in ar01v3-$n.yaml"
  grep -Fq "name: \"ar01v3-espnow-$n\"" "$cfg" \
    || fail "invalid node name in ar01v3-$n.yaml"
  grep -Fq "receiver_id: \"ar01v3_$n\"" "$cfg" \
    || fail "invalid receiver ID in ar01v3-$n.yaml"
done

# Run source-level regression and repository-hygiene checks. Publication mode
# additionally verifies that the private deployment secrets file is absent.
if python3 - "$ROOT_DIR" "$PUBLICATION_MODE" <<'PY'
from __future__ import annotations

import gzip
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
publication_mode = sys.argv[2] == "true"
base = root / "esphome" / "ar01v3-espnow-10x10-base.yaml"
text = base.read_text(encoding="utf-8")
errors: list[str] = []

required_base = (
    'project_name: "envpl.ar01v3_esp_rc01_gateway"',
    'project_version: "1.1.1"',
    'type: digest',
    'username: !secret web_server_username',
    'password: !secret web_server_password',
    'captive_portal:',
    'power_save_mode: NONE',
    'bluetooth_proxy:\n  active: true',
    'connection_slots: 2',
    'filter: 50us',
    'idle: 10ms',
    'tolerance: 50%',
    'on_rc_switch:',
    'Candidate %u/3: protocol=%u code=0x%llX',
    'id(rf_candidate_count) < 3',
    'saved as Princeton TX TE403',
    'on_nec:',
    'save_ir_raw(',
    'flipper_importer:',
    'js_url: ""',
    'js_include: ar01v3_web_v3.js',
    'include_internal: true',
    'components: [Flash_comp, flipper_importer]',
    'devices:',
    'id: ha_slot_actions_device',
    'id: group_remote_actions',
    'id: remote_button_actions',
    'type: std::array<uint8_t, 160>',
    'name: "Pilot"',
    'name: "Button"',
    'name: "Action"',
    'name: "Assignment"',
    'return id(remote_action_ui_ready) && !id(remote_action_ui_syncing);',
    'id(remote_dispatch_to_ha) = action == 0U',
    'id(espnow_pilot_1_button_event).trigger(button_name)',
    'id(espnow_pilot_10_button_event).trigger(button_name)',
    'id(flipper_web_importer).send_ir_slot(slot)',
    'id(flipper_web_importer).send_rf_slot(slot, repeats, gap_ms * 1000U)',
    'on_client_connected:\n    - delay: 10s',
    'condition:\n          api.connected:',
)
for marker in required_base:
    if marker not in text:
        errors.append(f"missing base configuration marker: {marker}")

for action in (
    "send_ir_slot", "send_rf_slot", "send_ir_raw", "send_ir_nec",
    "send_rf_raw", "send_rf_princeton", "send_rf_dooya",
    "transmit_ir_nec", "transmit_ir_raw", "transmit_rf_princeton",
    "transmit_rf_dooya", "transmit_rf_raw",
):
    if f"- action: {action}" not in text:
        errors.append(f"missing Home Assistant action: {action}")

for prefix, count in (("IR", 10), ("RF", 16)):
    for slot in range(count):
        if f'name: "Send {prefix} Slot {slot}"' not in text:
            errors.append(f"missing GUI button: Send {prefix} Slot {slot}")
        if f'name: "{prefix} Slot {slot}"' not in text:
            errors.append(f"missing slot preview: {prefix} Slot {slot}")

for pilot in range(1, 11):
    if f'name: "ESP-NOW Pilot {pilot} Button"' not in text:
        errors.append(f"missing per-pilot Home Assistant event entity: Pilot {pilot}")
    if f'id: espnow_pilot_{pilot}_button_event' not in text:
        errors.append(f"missing per-pilot event ID: Pilot {pilot}")
    for base_weight, marker in (
        (100, f'id: remote_battery_{pilot}'),
        (200, f'id: remote_paired_text_{pilot}'),
        (300, f'id: espnow_pilot_{pilot}_button_event'),
    ):
        block = text.split(marker, 1)[1].split("\n  - platform:", 1)[0]
        if f"sorting_weight: {base_weight + pilot}" not in block:
            errors.append(f"missing numeric web ordering after {marker}")

for package_name in (
    "esp_rc01_10x10_package.yaml",
    "esp_rc01_10x10_package_merge_named.yaml",
):
    package_text = (root / "home-assistant" / package_name).read_text(encoding="utf-8")
    if "- event: \"{{ 'esp_rc01_pilot_' ~ remote_slot ~ '_button' }}\"" in package_text:
        errors.append(f"templated event name remains in {package_name}")
    for pilot in range(1, 11):
        if f"- event: esp_rc01_pilot_{pilot}_button" not in package_text:
            errors.append(f"missing literal Pilot {pilot} event in {package_name}")
    if "esp_rc01_button" in package_text:
        errors.append(f"obsolete shared ESP-RC01 event remains in {package_name}")

blueprint_path = root / "home-assistant/blueprints/automation/envpl/esp_rc01_remote_actions.yaml"
if not blueprint_path.is_file():
    errors.append("missing ESP-RC01 Home Assistant blueprint")
else:
    blueprint_text = blueprint_path.read_text(encoding="utf-8")
    for marker in (
        "name: ESP-RC01 pilot button actions",
        "min_version: 2024.6.0",
        "event_type: !input pilot_event",
        "value: esp_rc01_pilot_1_button",
        "value: esp_rc01_pilot_10_button",
        "sequence: !input on_action",
        "sequence: !input p7_action",
    ):
        if marker not in blueprint_text:
            errors.append(f"missing Home Assistant blueprint marker: {marker}")
    for button_name in (
        "on", "off", "night", "brightness_up", "brightness_down",
        "p1", "p2", "p3", "p4", "p5", "p6", "p7",
    ):
        if f"trigger.event.data.button == '{button_name}'" not in blueprint_text:
            errors.append(f"missing Home Assistant blueprint action: {button_name}")
    if "collapsed: true" in blueprint_text:
        errors.append("Home Assistant blueprint input sections must be expanded by default")

installer_text = (root / "scripts/07_install_ha_package.sh").read_text(encoding="utf-8")
for marker in (
    "home-assistant/blueprints/automation/envpl/esp_rc01_remote_actions.yaml",
    'BLUEPRINT_DST_DIR="$HA_CONFIG/blueprints/automation/envpl"',
    'install -m 0644 "$BLUEPRINT" "$BLUEPRINT_DST"',
):
    if marker not in installer_text:
        errors.append(f"missing blueprint installer marker: {marker}")

if "timings: int[]" in text:
    errors.append("unsupported API variable type timings: int[] is present")
if text.count("on_raw:") != 1:
    errors.append("RF on_raw must be absent and the single IR on_raw handler must remain")
if 'name: "ESP-NOW Remote Raw"' in text or "espnow_remote_raw_event" in text:
    errors.append("obsolete shared ESP-NOW event entity is still present")

component = root / "esphome" / "components" / "flipper_importer"
for name in (
    "__init__.py", "flipper_parser.h", "flipper_page.h",
    "flipper_importer.h", "flipper_importer.cpp",
):
    if not (component / name).is_file():
        errors.append(f"missing flipper_importer component file: {name}")

importer_source = (component / "flipper_importer.cpp").read_text(encoding="utf-8")
if "/ar01v3-main-ui.js" in importer_source:
    errors.append("obsolete custom main-page JavaScript handler is still present")
if text.count('js_url: ""') != 1:
    errors.append("ESPHome js_url must stay empty while the local frontend is embedded")

action_select_block = text.split('name: "Action"', 1)[1].split("\n  - platform:", 1)[0]
if re.search(r"^      - script\.execute: remote_action_refresh_ui$", action_select_block, re.MULTILINE):
    errors.append("Action on_value must not restart remote_action_refresh_ui outside the syncing guard")

for entity_name, entity_id in (
    ("Assignment", "remote_action_status"),
    ("Pilot", "remote_action_pilot_select"),
    ("Button", "remote_action_button_select"),
    ("Action", "remote_action_select"),
):
    match = re.search(
        rf'  - platform: template\n    name: "{re.escape(entity_name)}"\n'
        rf'    id: {re.escape(entity_id)}(?P<body>.*?)(?=\n  - platform:|\Z)',
        text,
        re.DOTALL,
    )
    if match is None or "internal: true" not in match.group("body"):
        errors.append(f"local web control must be internal to the native API: {entity_name}")

main_ui = (root / "esphome" / "ar01v3_web_v3.js").read_text(encoding="utf-8")
for marker in (
    'var Qr=Object.defineProperty;',
    'G=Di([Rt("esp-entity-table")],G)',
    'name:"ESP-RC01 Button Assignment",sorting_weight:3',
    'name:"Flipper File Import",sorting_weight:4',
    'name:"IR Signals",sorting_weight:5',
    'name:"RF 433.92 MHz Signals",sorting_weight:6',
    's.name!=="Home Assistant Slot Buttons"',
    'i.name==="Flipper Import Page"',
    'window.location.assign("/flipper")',
    'input[data-field="timings"]',
    'style="display:flex;align-items:center;gap:8px;width:100%"',
    'style="flex:1 1 auto;width:auto;min-width:0"',
    'this._slotValuesReady=!1',
    'setTimeout(()=>{this._slotValuesReady=!0,this.requestUpdate()},8e3)',
    'this.entities.filter(s=>a.test(s.name)).length===26',
    '!c&&a.test(o.name)?"":o.state',
):
    if marker not in main_ui:
        errors.append(f"missing main-page UI marker: {marker}")
if any(marker in main_ui for marker in ("ar01v3StockUi", "patchEntityTableClass", "AR01_SLOT_COUNT")):
    errors.append("obsolete asynchronous frontend or DOM scanner is still present")
if any(marker in main_ui for marker in ("collectSlotRows", "stabilizeSlotRows", "setValuesVisible", "completeFrames")):
    errors.append("obsolete DOM-based slot stabilization is still present")

page_header = (component / "flipper_page.h").read_text(encoding="utf-8")
page_bytes = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", page_header))
try:
    page_html = gzip.decompress(page_bytes).decode("utf-8")
except Exception as exc:
    page_html = ""
    errors.append(f"cannot decompress the /flipper page: {exc}")
for marker in (
    "IMPORT &amp; TEST", "RF 433.92 MHz", "Live device status",
    "Princeton, Dooya and RAW OOK .sub files", "/flipper/status",
):
    if marker not in page_html:
        errors.append(f"missing /flipper page marker: {marker}")
if len(page_bytes) > 4096:
    errors.append("compressed /flipper page exceeds 4096 bytes")

secrets_example = (root / "esphome" / "secrets.example.yaml").read_text(encoding="utf-8")
for marker in ("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD", "GENERATED_BY_THE_CONFIGURATION_SCRIPT"):
    if marker not in secrets_example:
        errors.append(f"secrets.example.yaml is missing placeholder: {marker}")

required_files = (
    "README.md", "README_PL.md", "LICENSE", "AUTHORS.md",
    "THIRD_PARTY_NOTICES.md", "SECURITY.md", "CONTRIBUTING.md",
    "CHANGELOG.md", "GITHUB_PUBLISHING.md", "home-assistant/README.md",
    ".github/workflows/ci.yml", ".github/releases/v1.0.0.md",
    ".github/releases/v1.1.0.md",
    ".github/releases/v1.1.1.md",
    "home-assistant/blueprints/automation/envpl/esp_rc01_remote_actions.yaml",
    "docs/images/README.md", "docs/images/ar01v3-main-page.png",
    "docs/images/flipper-import-page.png",
    "docs/images/home-assistant-stored-actions.png", ".gitignore",
    "examples/princeton_example.sub", "examples/dooya_example.sub",
    "examples/nec_example.ir",
)
for relative in required_files:
    if not (root / relative).is_file():
        errors.append(f"missing publication file: {relative}")

for path in root.rglob("*"):
    if path.name == ".DS_Store" or path.name.startswith("._"):
        errors.append(f"macOS metadata file found: {path.relative_to(root)}")

if publication_mode and (root / "esphome" / "secrets.yaml").exists():
    errors.append("private esphome/secrets.yaml must not be present in the publication tree")

private_markers = (
    "HOME IOT", "/Users/", "/root/", "LED POWER", "SCREEN UP",
    "YAMAHA ON", "bartoszsupcinski",
)
for path in root.rglob("*"):
    if (not path.is_file() or path.name == "LICENSE" or
            path == root / "scripts" / "00_self_test.sh" or
            path == root / "esphome" / "secrets.yaml"):
        continue
    try:
        content = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        continue
    for marker in private_markers:
        if marker.lower() in content.lower():
            errors.append(f"publication marker {marker!r} found in {path.relative_to(root)}")

if errors:
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    raise SystemExit(1)
if publication_mode:
    print("OK: source regression and publication-hygiene checks")
else:
    print("OK: source regression and local-configuration hygiene checks")
PY
then
  :
else
  fail 'source regression checks failed'
fi

# Verify both fallback-AP password paths without creating private files in the repository.
DEFAULT_SECRET_TEST="$TMP_DIR/secrets-default"
SEPARATE_SECRET_TEST="$TMP_DIR/secrets-separate"
mkdir -p "$DEFAULT_SECRET_TEST/scripts" "$DEFAULT_SECRET_TEST/esphome"
mkdir -p "$SEPARATE_SECRET_TEST/scripts" "$SEPARATE_SECRET_TEST/esphome"
cp "$ROOT_DIR/scripts/02_configure_secrets.sh" "$DEFAULT_SECRET_TEST/scripts/"
cp "$ROOT_DIR/scripts/02_configure_secrets.sh" "$SEPARATE_SECRET_TEST/scripts/"

if printf 'Test WiFi\nTestWifiPassword\n\nWebPassword123!\nWebPassword123!\n\n' \
    | bash "$DEFAULT_SECRET_TEST/scripts/02_configure_secrets.sh" >/dev/null \
  && printf 'Test WiFi\nTestWifiPassword\n\nWebPassword123!\nWebPassword123!\nSeparateAP123!\nSeparateAP123!\n' \
    | bash "$SEPARATE_SECRET_TEST/scripts/02_configure_secrets.sh" >/dev/null \
  && python3 - "$DEFAULT_SECRET_TEST/esphome/secrets.yaml" \
      "$SEPARATE_SECRET_TEST/esphome/secrets.yaml" <<'PY'
import json
import stat
import sys
from pathlib import Path

def load(path_string):
    path = Path(path_string)
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        key, value = line.split(":", 1)
        values[key] = json.loads(value.strip())
    assert stat.S_IMODE(path.stat().st_mode) == 0o600
    return values

default = load(sys.argv[1])
separate = load(sys.argv[2])
assert default["fallback_ap_password"] == default["web_server_password"]
assert separate["fallback_ap_password"] == "SeparateAP123!"
assert separate["fallback_ap_password"] != separate["web_server_password"]
PY
then
  ok 'fallback AP password configuration'
else
  fail 'fallback AP password configuration failed'
fi

# Compile parser and component tests when a host C++ compiler is available.
if command -v g++ >/dev/null 2>&1; then
  if g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
      "$ROOT_DIR/tests/test_flipper_parser.cpp" -o "$TMP_DIR/test_flipper_parser" \
      && "$TMP_DIR/test_flipper_parser" \
          "$ROOT_DIR/examples/princeton_example.sub" \
          "$ROOT_DIR/examples/nec_example.ir" \
          "$ROOT_DIR/examples/dooya_example.sub"; then
    ok 'Flipper parser and NVS record tests'
  else
    fail 'Flipper parser tests failed'
  fi

  if g++ -std=c++20 -Wall -Wextra -Werror -pedantic \
      -I"$ROOT_DIR/tests/stubs" -I"$ROOT_DIR" \
      -c "$ROOT_DIR/esphome/components/flipper_importer/flipper_importer.cpp" \
      -o "$TMP_DIR/flipper_importer.o" \
    && g++ -std=c++20 -Wall -Wextra -Werror -pedantic \
      -I"$ROOT_DIR/tests/stubs" -I"$ROOT_DIR" \
      -c "$ROOT_DIR/esphome/components/Flash_comp/Flash_comp.cpp" \
      -o "$TMP_DIR/flash_comp.o" \
    && g++ -std=c++20 -Wall -Wextra -Werror -pedantic \
      -I"$ROOT_DIR/tests/stubs" -I"$ROOT_DIR" \
      "$ROOT_DIR/tests/test_component_compile.cpp" \
      "$TMP_DIR/flipper_importer.o" "$TMP_DIR/flash_comp.o" \
      -o "$TMP_DIR/test_component_compile" \
    && "$TMP_DIR/test_component_compile"; then
    ok 'host compilation of custom C++ components'
  else
    fail 'custom component host compilation failed'
  fi
else
  info 'g++ is unavailable; ESPHome compilation will perform the C++ checks'
fi

# Parse Python component files without leaving __pycache__ in the repository.
if python3 - "$ROOT_DIR" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
for path in (
    root / "esphome/components/Flash_comp/__init__.py",
    root / "esphome/components/flipper_importer/__init__.py",
    root / "scripts/generate_flipper_page.py",
):
    compile(path.read_text(encoding="utf-8"), str(path), "exec")
print("OK: Python source syntax")
PY
then
  :
else
  fail 'Python source syntax check failed'
fi

# Deep-parse YAML when PyYAML is available.
PYTHON_WITH_YAML=""
for py in /opt/esphome-10x10/bin/python python3; do
  if command -v "$py" >/dev/null 2>&1 && "$py" -c 'import yaml' >/dev/null 2>&1; then
    PYTHON_WITH_YAML="$py"
    break
  fi
done

if [[ -n "$PYTHON_WITH_YAML" ]]; then
  if "$PYTHON_WITH_YAML" - "$ROOT_DIR" <<'PY'
from __future__ import annotations

import sys
from pathlib import Path
import yaml

root = Path(sys.argv[1])

class Loader(yaml.SafeLoader):
    pass

def construct_unknown(loader, tag_suffix, node):
    if isinstance(node, yaml.ScalarNode):
        return loader.construct_scalar(node)
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node)
    return loader.construct_mapping(node)

Loader.add_multi_constructor("!", construct_unknown)

def construct_mapping(loader, node, deep=False):
    mapping = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if key in mapping:
            raise ValueError(f"duplicate key {key!r} on line {key_node.start_mark.line + 1}")
        mapping[key] = loader.construct_object(value_node, deep=deep)
    return mapping

Loader.construct_mapping = construct_mapping
for path in sorted(root.rglob("*.yaml")):
    relative = path.relative_to(root)
    if any(part in {".esphome", ".git", "__pycache__"} for part in relative.parts):
        continue
    if path.name == "secrets.yaml" or path.name.startswith("._"):
        continue
    with path.open("r", encoding="utf-8") as handle:
        yaml.load(handle, Loader=Loader)
print("OK: YAML files parse without duplicate keys")
PY
  then
    :
  else
    fail 'deep YAML parsing failed'
  fi
else
  info 'PyYAML is unavailable; ESPHome validation will perform full YAML parsing'
fi

if (( FAIL > 0 )); then
  printf 'Self-test completed with %d error(s).\n' "$FAIL" >&2
  exit 1
fi
printf '%s\n' 'Self-test completed successfully.'
