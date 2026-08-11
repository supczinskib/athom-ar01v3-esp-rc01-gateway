# Home Assistant files

The firmware exposes stored-signal buttons and parameterized ESPHome actions directly to Home Assistant. Device-specific mappings are intentionally not hard-coded in this repository: create covers, buttons, scripts, scenes, and automations in the Home Assistant user interface and assign whichever RF or IR slot belongs to your equipment.

## Files

- `esp_rc01_10x10_package.yaml` — complete package that deduplicates packets received by up to ten AR01V3 receivers from ten logical ESP-RC01 remotes and emits only pilot-specific `esp_rc01_pilot_N_button` events.
- `esp_rc01_10x10_package_merge_named.yaml` — the same package wrapped under a package key for installations that use `packages: !include_dir_merge_named packages/`.
- `blueprints/automation/envpl/esp_rc01_remote_actions.yaml` — one GUI automation per pilot with action fields for ON, OFF, Night, brightness, and P1–P7.
- `automation_examples.yaml` — optional examples that react to pilot-specific deduplicated events.

Only one deduplication package variant must be installed. The blueprint uses the pilot-specific events emitted by that package. See the root `README.md` for installation, pairing, GUI virtual-device creation, scene integration, and direct-transmission instructions.
