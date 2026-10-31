# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Pack one WakeNet preset into the application, without repartitioning."""
import argparse
from pathlib import Path
import struct
import zlib


def pack(source: Path) -> bytes:
    files = sorted(p for p in source.iterdir() if p.is_file())
    expected = {'_MODEL_INFO_', 'wn9_index', 'wn9_data'}
    if {p.name for p in files} != expected:
        raise ValueError('Unexpected model files')
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
    parser.add_argument('--compress', action='store_true')
    args = parser.parse_args()
    data = pack(args.source)
    if args.compress:
        data = struct.pack('<I', len(data)) + zlib.compress(data, 9)
    if not args.output.exists() or args.output.read_bytes() != data:
        args.output.write_bytes(data)
