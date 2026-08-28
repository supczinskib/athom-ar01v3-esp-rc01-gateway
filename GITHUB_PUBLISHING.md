# GitHub publishing metadata

Use the following values when creating the public repository.

## Repository

- Name: `athom-ar01v3-esp-rc01-gateway`
- URL: <https://github.com/supczinskib/athom-ar01v3-esp-rc01-gateway>
- Project title: `AR01V3 RF/IR, ESP-RC01 & Steinel NightmatIQ Plus Gateway`
- ESPHome project identifier: `envpl.ar01v3_esp_rc01_gateway`
- Author and maintainer: Bartosz Supcziński
- Contact: <bartek@env.pl>

## Description

> Community ESPHome gateway for Athom AR01V3: RF/IR, Flipper import, ESP-RC01 and Home Assistant integration for Steinel NightmatIQ Plus (IS Digi NM 2E6915).

## Suggested topics

`athom`, `athom-ar01v3`, `ar01v3`, `esp-rc01`, `esphome`, `esp32`, `home-assistant`, `flipper-zero`, `rf433`, `infrared`, `esp-now`, `bluetooth-mesh`, `steinel`, `nightmatiq`, `nightmatiq-plus`, `is-digi-nm-2e6915`

## CI badge

Both README files include the status badge for `.github/workflows/ci.yml`. It will report a result after the first workflow run on GitHub.

## Release 1.2.2

1. Run `bash scripts/00_self_test.sh --publication` in a clean tree.
2. Run `bash scripts/03_validate_all.sh` in a configured development tree.
3. Compile and hardware-test at least receiver 01.
4. Create tag `v1.2.2` from the tested commit.
5. Create a GitHub release named `AR01V3 RF/IR, ESP-RC01 & Steinel NightmatIQ Plus Gateway v1.2.2`.
6. Use `.github/releases/v1.2.2.md` as the release description.
7. Push the tested commit and tag, then create the release from the prepared
   release notes:

   ```bash
   git push origin main
   git push origin v1.2.2
   gh release create v1.2.2 --title "AR01V3 RF/IR, ESP-RC01 & Steinel NightmatIQ Plus Gateway v1.2.2" --notes-file .github/releases/v1.2.2.md
   ```

## Screenshots

The four current, author-approved screenshots are stored in `docs/images/`:

- `ar01v3-main-page.png` — the authenticated AR01V3 main page;
- `flipper-import-page.png` — the integrated `/flipper` page;
- `home-assistant-stored-actions.png` — the `AR01V3 Stored Signal Actions` device in Home Assistant;
- `steinel.png` — the optional Steinel NightmatIQ Plus integration page.

Both README files embed these images near the project overview. Replace them only with current screenshots that have passed the privacy checklist in `docs/images/README.md`.
