#!/usr/bin/env python3
"""Verify that ESPHome embedded the current main-page helper."""

from __future__ import annotations

import gzip
import hashlib
import re
import sys
from pathlib import Path


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {Path(sys.argv[0]).name} SOURCE_JS GENERATED_MAIN_CPP", file=sys.stderr)
        return 2

    source_path = Path(sys.argv[1])
    generated_path = Path(sys.argv[2])
    source = source_path.read_bytes()
    generated = generated_path.read_text(encoding="utf-8")
    match = re.search(
        r"ESPHOME_WEBSERVER_JS_INCLUDE\[\d+\].*?= \{([^}]*)\};",
        generated,
        re.DOTALL,
    )
    if match is None:
        print("ERROR: generated firmware source has no embedded main-page JavaScript", file=sys.stderr)
        return 1

    compressed = bytes(int(value) for value in re.findall(r"\d+", match.group(1)))
    try:
        embedded = gzip.decompress(compressed)
    except gzip.BadGzipFile as error:
        print(f"ERROR: embedded main-page JavaScript is not valid gzip: {error}", file=sys.stderr)
        return 1

    if embedded != source:
        print("ERROR: generated firmware contains stale main-page JavaScript", file=sys.stderr)
        print(f"  source:   {len(source)} bytes, sha256 {digest(source)}", file=sys.stderr)
        print(f"  embedded: {len(embedded)} bytes, sha256 {digest(embedded)}", file=sys.stderr)
        return 1

    print(f"OK: embedded main-page JavaScript: {len(source)} bytes, sha256 {digest(source)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
