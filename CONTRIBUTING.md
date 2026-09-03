# Contributing

Contributions are welcome. Review `THIRD_PARTY_NOTICES.md` before modifying material derived from upstream projects.

Before submitting a change:

1. Keep all source code, comments, user-interface text, logs, and technical documentation in English.
2. Do not commit credentials, unapproved device identifiers, private captured signals, build output, or macOS `._*` metadata files. Screenshots must be explicitly approved by the project author before publication.
3. Use neutral regression fixtures in `examples/`.
4. Preserve the AR01V3 pin assignment and avoid changing IR, ESP-NOW, Bluetooth Proxy, or Home Assistant behavior without documenting and testing the impact.
5. Run `bash scripts/00_self_test.sh` and `bash scripts/03_validate_all.sh`. Before packaging a release from a clean tree without local secrets, also run `bash scripts/00_self_test.sh --publication`.
6. Select a receiver with `bash scripts/02_configure.sh 01`, then compile it
   with `bash scripts/04_compile_one.sh`.
7. Describe hardware tests separately from host-side or configuration validation.

By contributing, you agree to license your contribution under `GPL-3.0-only` and certify that you have the right to submit it.
