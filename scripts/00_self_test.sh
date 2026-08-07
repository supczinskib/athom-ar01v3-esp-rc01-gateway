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
    'project_version: "1.0.0"',
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
    'components: [Flash_comp, flipper_importer]',
    'devices:',
    'id: ha_slot_actions_device',
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

if "timings: int[]" in text:
    errors.append("unsupported API variable type timings: int[] is present")
if text.count("on_raw:") != 1:
    errors.append("RF on_raw must be absent and the single IR on_raw handler must remain")

component = root / "esphome" / "components" / "flipper_importer"
for name in (
    "__init__.py", "flipper_parser.h", "flipper_page.h",
    "flipper_importer.h", "flipper_importer.cpp",
):
    if not (component / name).is_file():
        errors.append(f"missing flipper_importer component file: {name}")

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
    "YAMAHA ON", "bartoszsupcinski", "ChatGPT", "OpenAI", "Claude", "Codex",
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
