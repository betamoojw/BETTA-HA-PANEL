#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Generate or verify the boot and OTA emblem from the supplied brand JPEG."""

import argparse
from pathlib import Path
import zlib

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "docs/img/brand/BETTA-OS.jpg"
ASSETS = ROOT / "main/ui/assets"
PNG = ASSETS / "SMART86OS_Betta.png"
FALLBACK = ASSETS / "SMART86OS_Betta.c"
SIZE = (370, 340)
ARRAY_START = "uint8_t SMART86OS_Betta_map[] = {\n"


def color_data(image):
    """Round to RGB565 once, then expand those same bits for the PNG decoder."""
    rgb = image.convert("RGB").tobytes()
    packed = bytearray()
    expanded = bytearray()
    for r, g, b in zip(rgb[::3], rgb[1::3], rgb[2::3]):
        r, g, b = min(31, (r + 4) >> 3), min(63, (g + 2) >> 2), min(31, (b + 4) >> 3)
        word = (r << 11) | (g << 5) | b
        packed.extend((word & 255, word >> 8))
        expanded.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    return bytes(packed), bytes(expanded)


def fallback_source(current, packed):
    start = current.index(ARRAY_START) + len(ARRAY_START)
    end = current.index("};", start)
    stride = SIZE[0] * 2
    hex_bytes = tuple(f"0x{value:02x}" for value in range(256))
    rows = ("  " + ", ".join(hex_bytes[value] for value in packed[i:i + stride]) + ",\n"
            for i in range(0, len(packed), stride))
    return current[:start] + "".join(rows) + current[end:]


def generate(check):
    with Image.open(MASTER) as image:
        if image.size != SIZE:
            raise ValueError(f"{MASTER.name} must be {SIZE[0]} by {SIZE[1]} pixels")
        packed, expanded = color_data(image)
    current = FALLBACK.read_text(encoding="utf-8")
    expected = fallback_source(current, packed)
    if check:
        if current != expected:
            raise ValueError(f"{FALLBACK.name} differs from {MASTER.name}")
        with Image.open(PNG) as image:
            if (image.format != "PNG" or image.size != SIZE or image.mode != "RGB"
                    or image.tobytes() != expanded):
                raise ValueError(f"{PNG.name} differs from the RGB565 artwork")
    else:
        if current != expected:
            FALLBACK.write_text(expected, encoding="utf-8", newline="\n")
        Image.frombytes("RGB", SIZE, expanded).save(PNG, compress_level=9, compress_type=zlib.Z_DEFAULT_STRATEGY)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify both checked-in images")
    generate(parser.parse_args().check)
