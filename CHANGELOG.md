# Changelog

All notable project changes are documented here.

## Unreleased

- Added a Home Assistant blueprint that configures all actions of one logical ESP-RC01 pilot in a single GUI automation.
- Extended the Home Assistant package installer to install the blueprint automatically.
- Fixed pilot-specific Home Assistant event emission by using literal event names selected for logical Pilot 1 through Pilot 10.

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
