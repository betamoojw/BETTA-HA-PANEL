#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Pack and verify the pinned ESP-SR 2.4.7 Chinese custom-wake resource."""
import argparse
import hashlib
from pathlib import Path
import re
import struct
ROOT = Path(__file__).resolve().parent.parent

def packed_resource(source=None):
    if source is None:
        source = ROOT / 'managed_components/espressif__esp-sr/model/multinet_model/mn7_cn'
    # MultiNet7 create requires an initial FST command table, even when the
    # worker replaces it immediately. Keep its initial graph small and local.
    files = [(p.name, p.read_bytes()) for p in sorted(source.iterdir()) if p.is_file()]
    if {name for name, _ in files} != {'_MODEL_INFO_', 'mn7_index', 'mn7_data', 'vocab'}:
        raise ValueError('Unexpected MultiNet files')
    models = [('mn7_cn', files), ('fst', [('commands_cn.txt', b'1,ni hao xiao zhi\n')])]
    offset = 4 + sum(36 + 40 * len(files) for _, files in models)
    header = struct.pack('<I', len(models)); payload = b''
    for name, files in models:
        header += struct.pack('<32sI', name.encode(), len(files))
        for filename, data in files:
            header += struct.pack('<32sII', filename.encode(), offset, len(data))
            payload += data; offset += len(data)
    return header + payload

def verified_resource():
    header = (ROOT / 'main/xiaozhi/xiaozhi_custom_resource.h').read_text()
    expected = re.search(r'XIAOZHI_CUSTOM_SHA256 "([a-f0-9]{64})"', header)[1]
    size = int(re.search(r'XIAOZHI_CUSTOM_BYTES (\d+)U', header)[1])
    raw = packed_resource()
    if len(raw) != size or hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError('Installed ESP-SR model differs from the pinned resource')
    return raw, {'sha256': expected, 'bytes': size}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw, meta = verified_resource()
    if not args.output.exists() or args.output.read_bytes() != raw:
        args.output.write_bytes(raw)
    print(f"Custom wake model: {len(raw)} bytes, SHA256 {meta['sha256']}")
