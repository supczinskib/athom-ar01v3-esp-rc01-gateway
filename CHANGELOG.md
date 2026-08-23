# Changelog

All notable project changes are documented here.

## Unreleased

## 1.2.0 — 2026-08-23

- Added optional Steinel NightmatIQ Plus support to the normal firmware used by
  all ten AR01V3 receiver configurations.
- Added the authenticated `/steinel` page for selecting and importing a Steinel
  Cloud network without saving the account credentials.
- Added persistent controls for enabling, disabling and removing NightmatIQ
  while keeping Bluetooth Proxy as the default mode.
- Added immediate remove-and-reimport support and authenticated per-network IV
  Index caching without requiring a manual gateway restart.
- Streamed Steinel backups through the inactive OTA slot and parsed only the
  required records, keeping large cloud responses out of internal RAM.
- Added authenticated Bluetooth Mesh reads and writes for actual light output,
  illuminance, twilight threshold and operating mode.
- Added a separate Home Assistant device with resilient actual-output state,
  illuminance, threshold, mode, installed firmware, hardware revision,
  manufacturer, Company ID, Product ID, readiness, status and manual refresh.
- Kept the last confirmed output state across transient Mesh misses and marks it
  unavailable only after five minutes without a valid response, or immediately
  when NightmatIQ is disabled or removed.
- Added bounded retries for the two Home Assistant-critical reads and automatic
  discovery of the element that provides illuminance data.
- Read installed firmware and hardware revision from the Steinel manufacturer
  advertisement captured during gateway startup and verify its Product ID
  against authenticated Mesh Composition Data.
- Stabilized the constrained ESP32 runtime by deferring Home Assistant updates
  to the main loop, bounding API buffers and right-sizing Bluetooth Mesh, HTTP,
  Bluetooth and TLS resources while retaining the existing feature set.
- Added runtime, reset, heap and Mesh diagnostics to the NightmatIQ page.
- Preserved RF 433.92 MHz, IR, ESP-NOW, stored slots, Flipper import, OTA and the
  existing Home Assistant functionality in the unified firmware.
- Updated the English and Polish documentation and project screenshots.

## 1.1.2 — 2026-08-11

- Added a Home Assistant blueprint that configures all actions of one logical ESP-RC01 pilot in a single GUI automation.
- Extended the Home Assistant package installer to install the blueprint automatically.
- Fixed pilot-specific Home Assistant event emission by using literal event names selected for logical Pilot 1 through Pilot 10.
- Hid the ten pilot event entities from the embedded AR01V3 web page while retaining them in Home Assistant.

## 1.1.1 — 2026-08-10

- Added parsed Flipper `NECext` IR import with exact 16-bit address and command preservation.
- Added one Home Assistant event entity for each of the ten ESP-RC01 pilot slots, allowing identical buttons on different pilots to trigger different GUI automations.
- Added deduplicated pilot-specific events `esp_rc01_pilot_1_button` through `esp_rc01_pilot_10_button`.
- Removed the shared `ESP-NOW Remote Raw` entity and `esp_rc01_button` output; version 1.1.1 requires the supplied pilot-specific deduplication package.
- Applied explicit numerical web-interface ordering to pilot battery, pairing, and button-event rows so Pilot 10 follows Pilot 9.

## 1.1.0 — 2026-08-08

- Added persistent per-receiver assignments for all ten ESP-RC01 pilots and sixteen supported button events.
- Added compact `Pilot`, `Button`, `Action`, and `Assignment` controls to the authenticated main page.
- Added autonomous transmission from any stored IR or RF slot without requiring an active Home Assistant connection.
- Retained `Home Assistant` as the default route and added `Ignore` for buttons that must perform no local or Home Assistant action.
- Added a complete embedded ESPHome Web Server v3 frontend so the main page does not depend on an external JavaScript service.
- Kept the assignment, Flipper import, IR, and RF sections stable across initial loading and live updates.
- Kept IR/RF slot previews hidden until their complete state set is available, preventing temporary values from appearing under the wrong signal type.
- Preserved the `/flipper` shortcut, readable direct-IR timing controls, and Home Assistant-only stored-slot buttons without duplicating them on the device page.
- Added generated-resource verification to compilation, OTA, and USB upload scripts so stale embedded frontend code cannot be shipped.
- Changed fallback-AP setup to reuse the web-interface password by default, with an optional confirmed separate password.

## 1.0.0 — 2026-08-07

- Established the public project identity `AR01V3 ESP-RC01 Gateway`, repository name `athom-ar01v3-esp-rc01-gateway`, and ESPHome identifier `envpl.ar01v3_esp_rc01_gateway`.
- Added GitHub publication metadata, first-release notes, and an automated CI workflow for publication and host-side tests.
- Added a clear Flipper-to-AR01V3-to-Home Assistant project overview and a precise supported-format matrix.
- Added author-approved screenshots of the AR01V3 main page, integrated Flipper importer, and Home Assistant stored-signal actions.
- Added ten independent AR01V3 receiver configurations and ten logical ESP-RC01 pairing slots per receiver.
- Added centralized Home Assistant packet deduplication and the canonical `esp_rc01_button` event.
- Added persistent ESP-NOW pairing, battery reporting, button events, sequence data, receiver identity, and diagnostics.
- Added protocol-aware RF learning using three matching RC-Switch frames.
- Added the verified protocol-6 to Princeton transmit mapping used by compatible 24-bit remotes.
- Added ten IR and sixteen RF persistent signal slots shared by learning, Flipper import, local sending, and Home Assistant.
- Added NEC-first IR learning with RAW fallback.
- Added authenticated `/flipper` import and test interface for supported RF and IR files.
- Added direct Home Assistant actions for stored slots, NEC, IR RAW, Princeton, static Dooya, and RF RAW transmissions.
- Added twenty-six GUI-visible stored-slot buttons under the `AR01V3 Stored Signal Actions` sub-device.
- Added slot previews, configurable RF repeat count (default 5), repeat gap, and live status reporting.
- Retained ESPHome web interface, HTTP Digest authentication, Bluetooth Proxy, native API, OTA, diagnostics, safe mode, and factory reset.
- Added installation, validation, compilation, USB/OTA upload, log, and Home Assistant package scripts.
