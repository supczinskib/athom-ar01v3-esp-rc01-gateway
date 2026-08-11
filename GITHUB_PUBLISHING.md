# GitHub publishing metadata

Use the following values when creating the public repository.

## Repository

- Name: `athom-ar01v3-esp-rc01-gateway`
- URL: <https://github.com/supczinskib/athom-ar01v3-esp-rc01-gateway>
- Project title: `AR01V3 ESP-RC01 Gateway`
- ESPHome project identifier: `envpl.ar01v3_esp_rc01_gateway`
- Author and maintainer: Bartosz Supcziński
- Contact: <bartek@env.pl>

## Description

> Community ESPHome firmware for Athom AR01V3: Flipper RF/IR import, persistent signal slots, Home Assistant actions and ESP-RC01 multi-receiver support.

## Suggested topics

`athom`, `ar01v3`, `esp-rc01`, `esphome`, `esp32`, `home-assistant`, `flipper-zero`, `rf433`, `infrared`, `esp-now`

## CI badge

Both README files include the status badge for `.github/workflows/ci.yml`. It will report a result after the first workflow run on GitHub.

## First release

1. Run `bash scripts/00_self_test.sh --publication` in a clean tree.
2. Run `bash scripts/03_validate_all.sh` in a configured development tree.
3. Compile and hardware-test at least receiver 01.
4. Create tag `v1.1.1` from the tested commit.
5. Create a GitHub release named `AR01V3 ESP-RC01 Gateway v1.1.1`.
6. Use `.github/releases/v1.1.1.md` as the release description.
7. Attach the clean source archive and its SHA-256 checksum.

## Screenshots

The three current, author-approved screenshots are stored in `docs/images/`:

- `ar01v3-main-page.png` — the authenticated AR01V3 main page;
- `flipper-import-page.png` — the integrated `/flipper` page;
- `home-assistant-stored-actions.png` — the `AR01V3 Stored Signal Actions` device in Home Assistant.

Both README files embed these images near the project overview. Replace them only with current screenshots that have passed the privacy checklist in `docs/images/README.md`.
