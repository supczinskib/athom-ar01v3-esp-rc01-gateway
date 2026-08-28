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
generated_parts = {".esphome", ".git", "__pycache__"}


def is_generated(path: Path) -> bool:
    return any(part in generated_parts for part in path.relative_to(root).parts)

required_base = (
    'project_name: "envpl.ar01v3_esp_rc01_gateway"',
    'project_version: "1.2.2"',
    'type: digest',
    'username: !secret web_server_username',
    'password: !secret web_server_password',
    'captive_portal:',
    'power_save_mode: NONE',
    'channel: 6',
    'ap_timeout: 60s',
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
    'components: [Flash_comp, flipper_importer, nightmatiq_mesh]',
    'CONFIG_BLE_MESH_PROVISIONER: y',
    'CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY: y',
    'CONFIG_BTDM_CTRL_BLE_MAX_CONN: "2"',
    'CONFIG_BLE_MESH_ADV_BUF_COUNT: "6"',
    'CONFIG_BLE_MESH_WAIT_FOR_PROV_MAX_DEV_NUM: "1"',
    'CONFIG_BLE_MESH_MAX_PROV_NODES: "1"',
    'CONFIG_BLE_MESH_PBA_SAME_TIME: "1"',
    'CONFIG_BLE_MESH_LABEL_COUNT: "1"',
    'CONFIG_BLE_MESH_HEALTH_SRV: n',
    'CONFIG_BLE_MESH_PROXY: n',
    'CONFIG_BLE_MESH_TX_SEG_MSG_COUNT: "1"',
    'CONFIG_BLE_MESH_RX_SEG_MSG_COUNT: "1"',
    'CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM: "6"',
    'CONFIG_MBEDTLS_DYNAMIC_BUFFER: y',
    'nightmatiq_mesh:',
    'id: nightmatiq_bridge',
    'extended_diagnostics: true',
    'id: group_nightmatiq',
    'sorting_group_id: group_nightmatiq',
    'max_value: 1500',
    'return !id(nightmatiq_bridge).mesh_mode_enabled();',
    'name: "Steinel NightmatIQ Page"',
    'devices:',
    'id: ha_slot_actions_device',
    'id: nightmatiq_device',
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
if "CONFIG_BLE_MESH_CFG_CLI: y" not in text:
    errors.append("Bluetooth Mesh Configuration Client must remain enabled for live device identification")

component = root / "esphome" / "components" / "flipper_importer"
for name in (
    "__init__.py", "flipper_parser.h", "flipper_page.h",
    "flipper_importer.h", "flipper_importer.cpp",
):
    if not (component / name).is_file():
        errors.append(f"missing flipper_importer component file: {name}")

importer_source = (component / "flipper_importer.cpp").read_text(encoding="utf-8")
if "/ar01v3-main-ui.js" in importer_source or "/ar01v3-loader.js" in importer_source:
    errors.append("obsolete custom main-page JavaScript handler is still present")
if text.count('js_url: ""') != 1:
    errors.append("ESPHome js_url must stay empty while the local frontend is embedded")
if text.count("js_include: ar01v3_web_v3.js") != 1:
    errors.append("ESPHome must embed the proven local frontend from release 1.1.2")

nightmatiq = root / "esphome" / "components" / "nightmatiq_mesh"
for name in (
    "__init__.py", "nightmatiq_mesh.h", "nightmatiq_mesh.cpp",
    "nightmatiq_web.cpp", "nightmatiq_page.html", "nightmatiq_page.h",
):
    if not (nightmatiq / name).is_file():
        errors.append(f"missing nightmatiq_mesh component file: {name}")

nightmatiq_header_source = (nightmatiq / "nightmatiq_mesh.h").read_text(encoding="utf-8")
nightmatiq_python_source = (nightmatiq / "__init__.py").read_text(encoding="utf-8")
nightmatiq_mesh_source = (nightmatiq / "nightmatiq_mesh.cpp").read_text(encoding="utf-8")
nightmatiq_web_source = (nightmatiq / "nightmatiq_web.cpp").read_text(encoding="utf-8")
for marker, source in (
    ("FLAG_ENABLED", nightmatiq_header_source),
    ("FLAG_REMOVE_PENDING", nightmatiq_header_source),
    ("bool mesh_mode_enabled() const", nightmatiq_header_source),
    ("global_bluetooth_proxy->set_active(false)", nightmatiq_mesh_source),
    ("global_esp32_ble_tracker->stop_scan()", nightmatiq_mesh_source),
    ("global_ble->disable()", nightmatiq_mesh_source),
    ("advance_cloud_job_();", nightmatiq_mesh_source),
    ("cloud_session_reboot_pending_", nightmatiq_mesh_source),
    ("App.safe_reboot();", nightmatiq_mesh_source),
    ("new (std::nothrow) esp_ble_mesh_prov_t", nightmatiq_mesh_source),
    ("esp_ble_mesh_init(provision, &composition)", nightmatiq_mesh_source),
    ('url == "/steinel/enable"', nightmatiq_web_source),
    ('url == "/steinel/disable"', nightmatiq_web_source),
    ("request->beginResponse", nightmatiq_web_source),
    ("NIGHTMATIQ_PAGE_GZ", nightmatiq_web_source),
    ('response->addHeader("Content-Encoding", "gzip")', nightmatiq_web_source),
    ("save_enabled_(true)", nightmatiq_web_source),
    ("save_enabled_(false)", nightmatiq_web_source),
    ("saved configuration preserved", nightmatiq_web_source),
    ("Configuration removal is already in progress", nightmatiq_web_source),
    ("FLAG_ENABLED | FLAG_REMOVE_PENDING", nightmatiq_web_source),
    ("CLOUD_TASK_STACK_BYTES = 8192", nightmatiq_web_source),
    ("CLOUD_API_SHUTDOWN_TIMEOUT_MS = 5000", nightmatiq_web_source),
    ("cloud_pending_args_", nightmatiq_web_source),
    ("global_api_server->on_shutdown()", nightmatiq_web_source),
    ("global_api_server->teardown()", nightmatiq_web_source),
    ("ESPHome API stopped; releasing Bluetooth memory", nightmatiq_web_source),
    ("Starting cloud task after Bluetooth release", nightmatiq_web_source),
    ("esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE", nightmatiq_web_source),
    ("esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED", nightmatiq_web_source),
    ("schedule_cloud_session_reboot_", nightmatiq_web_source),
    ("CLOUD_DISCOVER_SESSION_TIMEOUT_MS", nightmatiq_web_source),
    ("cloud_free_after_ble", nightmatiq_web_source),
    ("largest_internal_block", nightmatiq_web_source),
    ("http_config.buffer_size = 1024", nightmatiq_web_source),
    ("http_config.buffer_size_tx = 512", nightmatiq_web_source),
    ("esp_ota_get_next_update_partition(nullptr)", nightmatiq_web_source),
    ("esp_partition_write(this->partition", nightmatiq_web_source),
    ("FlashJsonReader", nightmatiq_web_source),
    ("cloud_response_bytes", nightmatiq_web_source),
    ("model_1100", nightmatiq_web_source),
    ("selected->sensor_element_index", nightmatiq_web_source),
    ("loaded.onoff_address + 2", nightmatiq_web_source),
    ("class StatusJsonWriter", nightmatiq_web_source),
    ("httpd_resp_send_chunk", nightmatiq_web_source),
    ("USE_NIGHTMATIQ_EXTENDED_DIAGNOSTICS", nightmatiq_web_source),
    ("status_publish_pending_.store(true)", nightmatiq_web_source),
    ("status_publish_pending_.exchange(false)", nightmatiq_mesh_source),
    ('std::strcmp(key, "deviceKey") == 0', nightmatiq_web_source),
    ("ESP_BLE_MESH_MODEL_OP_COMPOSITION_DATA_GET", nightmatiq_mesh_source),
    ("live_version_id_", nightmatiq_header_source),
    ("send_device_revision_catalog_get_", nightmatiq_mesh_source),
    ("revision_catalog_in_flight_", nightmatiq_header_source),
    ("ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET", nightmatiq_mesh_source),
    ("ACTUAL_OUTPUT_STALE_MS = 5UL * 60UL * 1000UL", nightmatiq_header_source),
    ("record_actual_output_", nightmatiq_mesh_source),
    ("actual_output_binary_sensor_", nightmatiq_header_source),
    ("force_actual_output_unavailable_", nightmatiq_web_source),
    ("actual_output_known", nightmatiq_web_source),
    ("threshold_received", nightmatiq_web_source),
    ("threshold_centilux", nightmatiq_web_source),
    ("ADDRESS_POLICY_MAGIC", nightmatiq_header_source),
    ("ADDRESS_POLICY_FLAG_VERIFIED", nightmatiq_header_source),
    ("ADDRESS_POOL_TARGET_SIZE = 2048", nightmatiq_header_source),
    ("AUTO_ADDRESS_ROTATION_LIMIT = 16", nightmatiq_header_source),
    ("AUTO_ADDRESS_RECOVERY_DELAY_MS = 60000", nightmatiq_header_source),
    ("StoredAddressPolicy", nightmatiq_header_source),
    ("load_address_policy_", nightmatiq_web_source),
    ("save_address_policy_", nightmatiq_web_source),
    ("select_next_local_address_", nightmatiq_web_source),
    ("rotate_local_address_", nightmatiq_web_source),
    ("retryable_transport_error", nightmatiq_web_source),
    ("http_status <= 0", nightmatiq_web_source),
    ("retrying once with a fresh connection", nightmatiq_web_source),
    ("advance_address_recovery_", nightmatiq_mesh_source),
    ('std::strcmp(key, "lowAddress") == 0', nightmatiq_web_source),
    ('std::strcmp(key, "highAddress") != 0', nightmatiq_web_source),
    ("esp_read_mac(mac, ESP_MAC_WIFI_STA)", nightmatiq_web_source),
    ("esp_random()", nightmatiq_web_source),
    ("verified.flags |= ADDRESS_POLICY_FLAG_VERIFIED", nightmatiq_mesh_source),
    ("begin_access_operation_", nightmatiq_mesh_source),
    ("complete_access_operation_", nightmatiq_mesh_source),
    ("threshold_set_storage_", nightmatiq_header_source),
    ("control_request_pending_", nightmatiq_mesh_source),
    ("this->control_attempt_ < 2", nightmatiq_mesh_source),
    ("ESP_BLE_MESH_MODEL_OP_LIGHT_LC_MODE_SET_UNACK", nightmatiq_mesh_source),
    ("ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK", nightmatiq_mesh_source),
    ("ESP_BLE_MESH_MODEL_OP_SCENE_RECALL_UNACK", nightmatiq_mesh_source),
    ("MODE_CONFIRMATION_GRACE_MS", nightmatiq_mesh_source),
    ("threshold_override_pending_", nightmatiq_mesh_source),
    ("Rejected invalid NightmatIQ threshold", nightmatiq_mesh_source),
    ("lux > 1500.0f", nightmatiq_mesh_source),
    ("centilux <= 150000", nightmatiq_mesh_source),
    ("STEINEL_COMPANY_ID = 0x0563", nightmatiq_header_source),
    ("NIGHTMATIQ_PRODUCT_ID = 0x1DCE", nightmatiq_header_source),
    ("manufacturer.data.size() < 7", nightmatiq_mesh_source),
    ("esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA", nightmatiq_python_source),
    ("await esp32_ble_tracker.register_ble_device(var, config)", nightmatiq_python_source),
    ("AdvertisementParserType::RAW_ADVERTISEMENTS", nightmatiq_header_source),
    ("bool NightmatiqMesh::parse_devices", nightmatiq_mesh_source),
    ("ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE", nightmatiq_mesh_source),
    ("tracker->set_scan_own_address_type(BLE_ADDR_TYPE_RANDOM)", nightmatiq_mesh_source),
    ("tracker->set_scan_interval(IDENTITY_SCAN_INTERVAL_UNITS)", nightmatiq_mesh_source),
    ("tracker->start_scan()", nightmatiq_mesh_source),
    ("tracker->set_scan_own_address_type(BLE_ADDR_TYPE_PUBLIC)", nightmatiq_mesh_source),
    ("tracker->set_scan_interval(NORMAL_SCAN_INTERVAL_UNITS)", nightmatiq_mesh_source),
    ("esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED", nightmatiq_mesh_source),
    ("tracker->set_scan_window(NORMAL_SCAN_WINDOW_UNITS)", nightmatiq_mesh_source),
    ("tracker->set_scan_duration(NORMAL_SCAN_DURATION_SECONDS)", nightmatiq_mesh_source),
    ("IDENTITY_SCAN_WINDOW_MS = 30000", nightmatiq_mesh_source),
    ("COMPOSITION_BACKGROUND_RETRY_MS = 60000", nightmatiq_mesh_source),
    ("COMPOSITION_REQUEST_WATCHDOG_MS", nightmatiq_mesh_source),
    ("config_client.model->keys[0] = ESP_BLE_MESH_KEY_DEV", nightmatiq_mesh_source),
    ("nightmatiq_firmware_version", text),
    ("nightmatiq_hardware_version", text),
    ("nightmatiq_rssi", text),
    ("NightmatIQ Signal Strength", text),
    ("rssi_sensor_id", nightmatiq_python_source),
    ("set_rssi_sensor", nightmatiq_python_source),
    ("rssi_sensor_->publish_state", nightmatiq_mesh_source),
    ("nightmatiq_actual_output", text),
    ("nightmatiq_manufacturer", text),
    ("nightmatiq_company_id", text),
    ("nightmatiq_product_id", text),
    ('device_id: nightmatiq_device', text),
    ('0x%04x (Steinel GmbH)', nightmatiq_mesh_source),
    (r'\"company_id\":\"%04x\",\"product_id\":\"%04x\"', nightmatiq_web_source),
    ("static_cast<httpd_req_t *>(*request)", nightmatiq_web_source),
    ("cv.Optional(CONF_EXTENDED_DIAGNOSTICS, default=True)", nightmatiq_python_source),
    ('cg.add_define("USE_NIGHTMATIQ_EXTENDED_DIAGNOSTICS")', nightmatiq_python_source),
):
    if marker not in source:
        errors.append(f"missing NightmatIQ unified-mode marker: {marker}")
if "provision.prov_unicast_addr =" in nightmatiq_mesh_source:
    errors.append("ESP-IDF const prov_unicast_addr must be initialized, not assigned")
if "global_ble->enable()" in nightmatiq_mesh_source or "cloud_ble_resume_pending_" in nightmatiq_header_source + nightmatiq_mesh_source + nightmatiq_web_source:
    errors.append("NightmatIQ cloud flow must recover Bluetooth only through a controlled reboot")
if "NET_BUF_SIMPLE_DEFINE(value, 3)" in nightmatiq_mesh_source:
    errors.append("NightmatIQ Light LC SET must not reference a stack-backed property buffer")
if "std::string NightmatiqMesh::status_json_" in nightmatiq_web_source:
    errors.append("NightmatIQ status must be streamed without a heap-backed full JSON document")
if "body.reserve(1536" in nightmatiq_web_source:
    errors.append("NightmatIQ status must not reserve a large dynamic response buffer")
onoff_set_block = nightmatiq_mesh_source.split("bool NightmatiqMesh::send_onoff_set_", 1)[1].split(
    "bool NightmatiqMesh::send_scene_recall_", 1
)[0]
if "this->config_.onoff_address" not in onoff_set_block or "this->config_.lc_address" in onoff_set_block:
    errors.append("NightmatIQ Generic OnOff SET must target the primary OnOff element")
for obsolete in (
    "esp_ble_mesh_register_ble_callback", "esp_ble_mesh_start_ble_scanning",
    "advance_mesh_identity_listener_", "IDENTITY_REFRESH_INTERVAL_MS",
):
    if obsolete in nightmatiq_mesh_source:
        errors.append(f"obsolete passive NightmatIQ identity listener is still present: {obsolete}")
if 'xTaskCreate(cloud_task_, "steinel_cloud", 12288' in nightmatiq_web_source:
    errors.append("oversized NightmatIQ cloud task stack is still present")
if 'url == "/steinel/rotate-address"' in nightmatiq_web_source:
    errors.append("automatic NightmatIQ address recovery must not add a manual web endpoint")
if "address_pool_low" in nightmatiq_web_source or "automatic_address_rotations" in nightmatiq_web_source:
    errors.append("NightmatIQ address recovery internals must not alter the public status JSON")
if 'static_cast<std::string *>(event->user_data)' in nightmatiq_web_source:
    errors.append("NightmatIQ cloud responses must not grow an unchecked std::string")
if "send_json_(request, 200, this->status_json_())" in nightmatiq_web_source:
    errors.append("NightmatIQ status must not create a second full JSON response copy")
start_cloud_job_block = nightmatiq_web_source.split("bool NightmatiqMesh::start_cloud_job_", 1)[1].split(
    "void NightmatiqMesh::advance_cloud_job_", 1
)[0]
if "xTaskCreate" in start_cloud_job_block:
    errors.append("NightmatIQ HTTPS task must be created only after Bluetooth has released memory")
address_recovery_block = nightmatiq_mesh_source.split(
    "void NightmatiqMesh::advance_address_recovery_", 1
)[1].split("void NightmatiqMesh::keys_bound_", 1)[0]
if "!this->identity_found_this_boot_.load()" not in address_recovery_block:
    errors.append("automatic Mesh address recovery must require a current-boot NightmatIQ report")

# Exercise the pure address-selection rules independently of the embedded
# runtime. This catches off-by-one errors at 0x7FFF, occupied element ranges,
# one-address pools and wraparound before firmware reaches a device.
def derive_pool(low: int, high: int, occupied: list[tuple[int, int]]) -> tuple[int, int] | None:
    if low <= 0 or low > high or high >= 0x8000:
        return None
    pool_high = min(high, 0x7FFE)
    highest_occupied = low - 1
    for address, elements in occupied:
        node_last = min(address + max(1, elements) - 1, 0x7FFF)
        if address <= high and node_last >= low:
            highest_occupied = max(highest_occupied, min(node_last, high))
    bounded_pool_low = pool_high - 2048 + 1 if pool_high >= 2048 else low
    pool_low = max(low, bounded_pool_low, highest_occupied + 1)
    return None if pool_low > pool_high else (pool_low, pool_high)


def next_address(pool: tuple[int, int], current: int) -> int | None:
    first, last = pool
    if first >= last:
        return None
    candidate = last if current <= first else current - 1
    return None if candidate == current else candidate


if derive_pool(0x0001, 0x199A, [(0x0001, 3)]) != (0x119B, 0x199A):
    errors.append("NightmatIQ address pool does not match the verified Home-network case")
if derive_pool(0x0001, 0x7FFF, [(0x0001, 3)]) != (0x77FF, 0x7FFE):
    errors.append("NightmatIQ address pool must reserve 0x7FFF for provisioner start-address arithmetic")
if derive_pool(0x1000, 0x1002, [(0x1000, 3)]) is not None:
    errors.append("NightmatIQ address pool must reject a fully occupied provisioner range")
if next_address((0x119B, 0x199A), 0x146C) != 0x146B:
    errors.append("NightmatIQ address recovery must descend through the saved pool")
if next_address((0x119B, 0x199A), 0x119B) != 0x199A:
    errors.append("NightmatIQ address recovery must wrap inside the saved pool")
if next_address((0x199A, 0x199A), 0x199A) is not None:
    errors.append("NightmatIQ address recovery must stop for a one-address pool")

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
    'name:"Steinel NightmatIQ Plus",sorting_weight:5',
    'name:"IR Signals",sorting_weight:6',
    'name:"RF 433.92 MHz Signals",sorting_weight:7',
    's.name!=="Home Assistant Slot Buttons"',
    'i.name==="Flipper Import Page"',
    'window.location.assign("/flipper")',
    'i.name==="Steinel NightmatIQ Page"',
    'window.location.assign("/steinel")',
    's.device==="Steinel NightmatIQ Plus"&&s.name==="NightmatIQ Status"',
    'o.name.replace(/^NightmatIQ /,"")',
    'o.name==="Steinel NightmatIQ Page"?"Configuration Page"',
    'o.name==="Steinel NightmatIQ Page"||o.name==="NightmatIQ Status"',
    'input[data-field="timings"]',
    'style="display:flex;align-items:center;gap:8px;width:100%"',
    'style="flex:1 1 auto;width:auto;min-width:0"',
    'this._slotValuesReady=!1',
    'setTimeout(()=>{this._slotValuesReady=!0,this.requestUpdate()},8e3)',
    'this.entities.filter(s=>a.test(s.name)).length===26',
    '!c&&a.test(o.name)?"":o.state',
    '.filter(s=>!/^ESP-NOW Pilot (?:[1-9]|10) Button$/.test(s.name))',
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

nightmatiq_header = (nightmatiq / "nightmatiq_page.h").read_text(encoding="utf-8")
nightmatiq_bytes = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", nightmatiq_header))
try:
    nightmatiq_html = gzip.decompress(nightmatiq_bytes).decode("utf-8")
except Exception as exc:
    nightmatiq_html = ""
    errors.append(f"cannot decompress the /steinel page: {exc}")
for marker in (
    "Steinel NightmatIQ Plus", "/steinel/discover", "/steinel/install",
    "/steinel/enable", "/steinel/disable", "Credentials are never saved",
    "Disabling NightmatIQ keeps the saved configuration",
    "Twilight threshold", "Last Mesh RSSI", "meshRssi", " · Confirmed", "Company ID", "Product ID",
    "ENABLE NIGHTMATIQ", "DISABLE NIGHTMATIQ", "REMOVE CONFIGURATION",
):
    if marker not in nightmatiq_html:
        errors.append(f"missing /steinel page marker: {marker}")
if nightmatiq_html and nightmatiq_html != (nightmatiq / "nightmatiq_page.html").read_text(encoding="utf-8"):
    errors.append("embedded /steinel page differs from nightmatiq_page.html")
for obsolete in (
    "Firmware source", "Hardware source", "Composition Version ID",
    "Live identity authorization", "Live Company ID", "Live Product ID",
    "confirmed live", "Leave IV Index at 0",
):
    if obsolete in nightmatiq_html:
        errors.append(f"obsolete /steinel page text is still present: {obsolete}")
if len(nightmatiq_bytes) > 8192:
    errors.append("compressed /steinel page exceeds 8192 bytes")

for marker, source in (
    ("context.recv_rssi", nightmatiq_mesh_source),
    ("mesh_rssi_received_", nightmatiq_header_source),
    ("mesh_last_rssi_dbm", nightmatiq_web_source),
    ("mesh_last_rssi_age_seconds", nightmatiq_web_source),
):
    if marker not in source:
        errors.append(f"missing NightmatIQ Mesh RSSI marker: {marker}")

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
    ".github/releases/v1.1.1.md", ".github/releases/v1.1.2.md",
    ".github/releases/v1.2.0.md", ".github/releases/v1.2.1.md",
    ".github/releases/v1.2.2.md",
    "home-assistant/blueprints/automation/envpl/esp_rc01_remote_actions.yaml",
    "home-assistant/nightmatiq_dashboard_card.yaml",
    "docs/NIGHTMATIQ.md", "docs/NIGHTMATIQ_PL.md",
    "docs/images/README.md", "docs/images/ar01v3-main-page.png",
    "docs/images/flipper-import-page.png",
    "docs/images/home-assistant-stored-actions.png",
    "docs/images/steinel.png", ".gitignore",
    "scripts/patch_esphome_api.py",
    "scripts/patch_esphome_ble_tracker.py",
    "examples/princeton_example.sub", "examples/dooya_example.sub",
    "examples/nec_example.ir",
)
for relative in required_files:
    if not (root / relative).is_file():
        errors.append(f"missing publication file: {relative}")

if (root / "esphome" / "ar01v3-nightmatiq-experimental.yaml").exists():
    errors.append("separate NightmatIQ firmware entry point must not exist")

base_yaml = (root / "esphome" / "ar01v3-espnow-10x10-base.yaml").read_text(encoding="utf-8")
if "  max_send_queue: 8\n" not in base_yaml:
    errors.append("ESPHome API send queue must retain the ESP32 depth needed for entity discovery")
for marker in (
    "    id: nightmatiq_mode\n",
    "    restore_value: true\n",
    "    initial_option: \"Auto\"\n",
):
    if marker not in base_yaml:
        errors.append(f"NightmatIQ mode startup state is missing: {marker.strip()}")

if publication_mode:
    generated_build_dirs = sorted({
        path.relative_to(root)
        for name in (".esphome", "build", "dist")
        for path in root.rglob(name)
        if path.is_dir() and not any(part in {".git", "__pycache__"} for part in path.relative_to(root).parts)
    })
    for path in generated_build_dirs:
        errors.append(f"generated build directory found: {path}")

for path in root.rglob("*"):
    if is_generated(path):
        continue
    if path.name == ".DS_Store" or path.name.startswith("._"):
        errors.append(f"macOS metadata file found: {path.relative_to(root)}")

if publication_mode and (root / "esphome" / "secrets.yaml").exists():
    errors.append("private esphome/secrets.yaml must not be present in the publication tree")

private_markers = (
    "HOME IOT", "/Users/", "/root/", "LED POWER", "SCREEN UP",
    "YAMAHA ON", "bartoszsupcinski",
)
for path in root.rglob("*"):
    if (is_generated(path) or not path.is_file() or path.name == "LICENSE" or
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

# Exercise the ESPHome API patch against the exact supported upstream snippet,
# including its idempotent second invocation.
API_PATCH_TEST="$TMP_DIR/api_overflow_buffer.cpp"
if python3 - "$API_PATCH_TEST" <<'PY'
from pathlib import Path
import sys

Path(sys.argv[1]).write_text(
    '#include "api_overflow_buffer.h"\n'
    '#ifdef USE_API\n'
    '#include <cstring>\n\n'
    'namespace esphome::api {\n'
    'bool test(uint16_t buffer_size) {\n'
    '  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)\n'
    '  auto *entry = new Entry{new uint8_t[buffer_size], buffer_size, 0};\n'
    '  this->queue_[this->tail_] = entry;\n'
    '}\n}\n'
    '#endif\n',
    encoding='utf-8',
)
Path(sys.argv[1]).with_suffix('.h').write_text(
    '#pragma once\n'
    '#include <array>\n'
    '#include <cstdint>\n'
    '#include <sys/types.h>\n'
    'struct Entry {\n'
    '  unsigned char *data;\n'
    '  static void destroy(Entry *entry) {\n'
    '      delete[] entry->data;\n'
    '      delete entry;  // NOLINT(cppcoreguidelines-owning-memory)\n'
    '  }\n'
    '};\n',
    encoding='utf-8',
)
PY
  python3 "$ROOT_DIR/scripts/patch_esphome_api.py" --source "$API_PATCH_TEST" >/dev/null \
  && python3 "$ROOT_DIR/scripts/patch_esphome_api.py" --source "$API_PATCH_TEST" >/dev/null \
  && grep -Fq 'std::malloc(buffer_size)' "$API_PATCH_TEST" \
  && grep -Fq 'std::free(entry->data)' "${API_PATCH_TEST%.cpp}.h"; then
  ok 'ESPHome API low-memory safety patch'
else
  fail 'ESPHome API low-memory safety patch failed'
fi

# Exercise the pinned ESPHome BLE tracker patch and its idempotent second
# invocation. NightmatIQ needs a random scanner address, while upstream 2026.7.3
# otherwise hard-codes a public address in start_scan_().
BLE_PATCH_TEST="$TMP_DIR/esp32_ble_tracker.cpp"
if python3 - "$BLE_PATCH_TEST" <<'PY'
from pathlib import Path
import sys

Path(sys.argv[1]).write_text(
    'void start_scan() {\n'
    '  this->scan_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;\n'
    '}\n',
    encoding='utf-8',
)
Path(sys.argv[1]).with_suffix('.h').write_text(
    '  void set_scan_active(bool scan_active) { scan_active_ = scan_active; }\n'
    '  ScannerState scanner_state_{ScannerState::IDLE};\n',
    encoding='utf-8',
)
PY
  python3 "$ROOT_DIR/scripts/patch_esphome_ble_tracker.py" --source "$BLE_PATCH_TEST" >/dev/null \
  && python3 "$ROOT_DIR/scripts/patch_esphome_ble_tracker.py" --source "$BLE_PATCH_TEST" >/dev/null \
  && grep -Fq 'this->scan_params_.own_addr_type = this->scan_own_address_type_;' "$BLE_PATCH_TEST" \
  && grep -Fq 'set_scan_own_address_type(esp_ble_addr_type_t address_type)' "${BLE_PATCH_TEST%.cpp}.h" \
  && grep -Fq 'scan_own_address_type_{BLE_ADDR_TYPE_PUBLIC}' "${BLE_PATCH_TEST%.cpp}.h"; then
  ok 'ESPHome BLE scanner address patch'
else
  fail 'ESPHome BLE scanner address patch failed'
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
if [[ "${AR01V3_SKIP_HOST_COMPILE:-false}" == true ]]; then
  info 'host C++ compilation skipped by AR01V3_SKIP_HOST_COMPILE'
elif command -v g++ >/dev/null 2>&1; then
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
    root / "esphome/components/nightmatiq_mesh/__init__.py",
    root / "scripts/generate_flipper_page.py",
    root / "scripts/generate_nightmatiq_page.py",
    root / "scripts/patch_esphome_api.py",
    root / "scripts/patch_esphome_ble_tracker.py",
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
