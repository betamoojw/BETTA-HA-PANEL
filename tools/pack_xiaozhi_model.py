# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Pack one pinned ESP-SR model into the application, without repartitioning."""
import argparse
from pathlib import Path
import struct


def pack(source: Path) -> bytes:
    files = sorted(p for p in source.iterdir() if p.is_file())
    assert {p.name for p in files} == {'_MODEL_INFO_', 'wn9_index', 'wn9_data'}
    header = struct.pack('<I32sI', 1, source.name.encode(), len(files))
    offset = len(header) + 40 * len(files)
    payload = b''
    for path in files:
        data = path.read_bytes()
        header += struct.pack('<32sII', path.name.encode(), offset, len(data))
        payload += data
        offset += len(data)
    return header + payload


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data = pack(args.source)
    if not args.output.exists() or args.output.read_bytes() != data:
        args.output.write_bytes(data)
