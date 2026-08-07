# Changelog

All notable project changes are documented here.

## 1.0.0 — 2026-08-07

- Established the public project identity `AR01V3 ESP-RC01 Gateway`, repository name `athom-ar01v3-esp-rc01-gateway`, and ESPHome identifier `envpl.ar01v3_esp_rc01_gateway`.
- Added GitHub publication metadata, first-release notes, and an automated CI workflow for publication and host-side tests.
- Added a clear Flipper-to-AR01V3-to-Home Assistant project overview and a precise supported-format matrix.
- Added author-approved screenshots of the AR01V3 main page, integrated Flipper importer, and Home Assistant stored-signal actions.
- Added ten independent AR01V3 receiver configurations and ten logical ESP-RC01 pairing slots per receiver.
- Added centralized Home Assistant packet deduplication and the canonical `esp_rc01_button` event.
- Added persistent ESP-NOW pairing, battery reporting, hold events, sequence data, receiver identity, and diagnostics.
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
