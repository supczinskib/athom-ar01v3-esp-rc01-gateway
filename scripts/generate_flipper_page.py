#!/usr/bin/env python3
from __future__ import annotations

import gzip
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = root / "esphome/components/flipper_importer/flipper_page.html"
target = root / "esphome/components/flipper_importer/flipper_page.h"
raw = source.read_bytes()
compressed = gzip.compress(raw, compresslevel=9, mtime=0)
rows = []
for offset in range(0, len(compressed), 16):
    rows.append("  " + ", ".join(f"0x{value:02x}" for value in compressed[offset : offset + 16]) + ",")
target.write_text(
    "#pragma once\n#include <cstddef>\n#include <cstdint>\n"
    "namespace esphome::flipper_importer {\n"
    f"static constexpr size_t FLIPPER_PAGE_RAW_SIZE = {len(raw)};\n"
    f"static const uint8_t FLIPPER_PAGE_GZ[{len(compressed)}] = {{\n"
    + "\n".join(rows)
    + "\n};\n}  // namespace esphome::flipper_importer\n",
    encoding="utf-8",
)
print(f"Generated {target.name}: {len(raw)} -> {len(compressed)} bytes")
