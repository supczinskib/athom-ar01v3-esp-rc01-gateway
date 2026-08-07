# Home Assistant files

The firmware exposes stored-signal buttons and parameterized ESPHome actions directly to Home Assistant. Device-specific mappings are intentionally not hard-coded in this repository: create covers, buttons, scripts, scenes, and automations in the Home Assistant user interface and assign whichever RF or IR slot belongs to your equipment.

## Files

- `esp_rc01_10x10_package.yaml` — complete package that deduplicates packets received by up to ten AR01V3 receivers from ten logical ESP-RC01 remotes.
- `esp_rc01_10x10_package_merge_named.yaml` — the same package wrapped under a package key for installations that use `packages: !include_dir_merge_named packages/`.
- `automation_examples.yaml` — optional examples that react to the canonical `esp_rc01_button` event.

Only one deduplication package variant must be installed. See the root `README.md` for installation, pairing, GUI virtual-device creation, scene integration, and direct-transmission instructions.
