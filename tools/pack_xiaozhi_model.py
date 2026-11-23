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


def compress_model(data: bytes) -> bytes:
    # Quantized weights can compress better with filtered DEFLATE. Keep the
    # default candidate so other models never grow; both use the existing zlib
    # envelope and firmware decoder without additional runtime allocations.
    filtered = zlib.compressobj(level=9, memLevel=6, strategy=zlib.Z_FILTERED)
    candidates = (zlib.compress(data, 9), filtered.compress(data) + filtered.flush())
    compressed = min(candidates, key=len)
    if zlib.decompress(compressed) != data:
        raise ValueError('Compressed model did not round-trip')
    return struct.pack('<I', len(data)) + compressed


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--compress', action='store_true')
    args = parser.parse_args()
    data = pack(args.source)
    if args.compress:
        data = compress_model(data)
    if not args.output.exists() or args.output.read_bytes() != data:
        args.output.write_bytes(data)
