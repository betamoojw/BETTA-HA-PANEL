#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Package a C6 image as BC6Z + LE32 size + SHA256 + a zlib stream."""
import argparse
import hashlib
from pathlib import Path
import struct
import zlib


def pack(data):
    if not 288 <= len(data) <= 4 * 1024 * 1024:
        raise ValueError("C6 image must be between 288 bytes and 4 MiB")
    if data[0] != 0xE9 or struct.unpack_from("<H", data, 12)[0] != 13:
        raise ValueError("Not an ESP32-C6 firmware image")
    if struct.unpack_from("<I", data, 32)[0] != 0xABCD5432:
        raise ValueError("Missing ESP application descriptor")
    compressed = zlib.compress(data, 9)
    if zlib.decompress(compressed) != data:
        raise ValueError("C6 compression round trip failed")
    return b"BC6Z" + struct.pack("<I", len(data)) + hashlib.sha256(data).digest() + compressed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = args.source.read_bytes()
    archive = pack(data)
    if not args.output.exists() or args.output.read_bytes() != archive:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(archive)
    print(f"C6 image: {len(data)} -> {len(archive)} bytes, SHA256 {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
