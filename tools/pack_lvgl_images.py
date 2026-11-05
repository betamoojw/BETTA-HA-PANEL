#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Losslessly pack maintained RGB565/A8 C arrays for LVGL's built-in LZ4 decoder.

Only build outputs are rewritten. No image conversion, new Python dependencies,
filesystem resources or persistent allocation is needed on the panel.
Block format: https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md
"""
import argparse
from pathlib import Path
import re
import struct

SOURCES = ('ui/assets/Temp.c', 'ui/assets/RGB.c', 'ui/assets/visiblecolor.c',
           'ui/fonts/notification_emoji_data.c')
ARRAY = re.compile(r'(uint8_t\s+(\w+)\[)(\d*)(\]\s*=\s*\{)(.*?)(\};)', re.S)


def lz4_block(data):
    """Deterministic greedy encoder; LVGL supplies the production safe decoder."""
    output = bytearray()
    previous = {}
    anchor = pos = 0

    def length_extra(length):
        while length >= 255:
            output.append(255)
            length -= 255
        output.append(length)

    # LZ4 requires the last match to start >=12 bytes before EOF and the last
    # five bytes to be literals, including for overlapping match copies.
    while pos <= len(data) - 12:
        key = data[pos:pos + 4]
        match = previous.get(key)
        previous[key] = pos
        if match is None or pos - match > 65535:
            pos += 1
            continue
        end = pos + 4
        while end < len(data) - 5 and data[end] == data[match + end - pos]:
            end += 1
        literals, matched = pos - anchor, end - pos - 4
        output.append((min(literals, 15) << 4) | min(matched, 15))
        if literals >= 15:
            length_extra(literals - 15)
        output.extend(data[anchor:pos])
        output.extend(struct.pack('<H', pos - match))
        if matched >= 15:
            length_extra(matched - 15)
        for i in range(pos + 1, end):
            previous[data[i:i + 4]] = i
        pos = anchor = end
    literals = len(data) - anchor
    output.append(min(literals, 15) << 4)
    if literals >= 15:
        length_extra(literals - 15)
    output.extend(data[anchor:])
    return bytes(output)


def pack_source(text):
    # The maintained descriptors have implicit zero flags. Do not overwrite a
    # future premultiplied/modifiable/compressed flag with COMPRESSED alone.
    if re.search(r'\.(?:header\s*\.\s*)?flags\s*=', text):
        raise ValueError('Explicit image flags require a reviewed descriptor conversion')
    sizes = {}

    def array(match):
        tokens = re.findall(r'0x([0-9a-fA-F]{2})\b', match[5])
        if re.sub(r'0x[0-9a-fA-F]{2}\b|[\s,]', '', match[5]):
            raise ValueError('Unsupported image array: ' + match[2])
        raw = bytes(int(value, 16) for value in tokens)
        if not raw or (match[3] and int(match[3]) != len(raw)):
            raise ValueError('Image array length mismatch: ' + match[2])
        block = lz4_block(raw)
        packed = struct.pack('<III', 2, len(block), len(raw)) + block
        if len(packed) >= len(raw):
            return match[0]
        sizes[match[2]] = (len(raw), len(packed))
        lines = ['    ' + ','.join(f'0x{b:02x}' for b in packed[i:i + 24]) + ','
                 for i in range(0, len(packed), 24)]
        return match[1] + str(len(packed)) + match[4] + '\n' + '\n'.join(lines) + '\n' + match[6]

    output, count = ARRAY.subn(array, text)
    if not count or not sizes:
        raise ValueError('No compressible image arrays')
    for name in sizes:
        # Both maintained formats place data_size/data after the image header.
        # Reject unfamiliar descriptors rather than silently changing pixels.
        pattern = r'\.data_size\s*=\s*[^,]+,\s*\.data\s*=\s*' + re.escape(name) + r'\b'
        replacement = '.header.flags = LV_IMAGE_FLAGS_COMPRESSED, .data_size = sizeof(' + name + '), .data = ' + name
        output, replaced = re.subn(pattern, replacement, output)
        if replaced != 1:
            raise ValueError('Expected one image descriptor: ' + name)
    return '/* Build-only lossless LZ4 encoding; original attribution follows. */\n' + output, sizes


def generate(root, out):
    out.mkdir(parents=True, exist_ok=True)
    raw_total = packed_total = 0
    for relative in SOURCES:
        source = root / 'main' / relative
        text, sizes = pack_source(source.read_text(encoding='utf-8'))
        target = out / source.name
        if not target.exists() or target.read_text(encoding='utf-8') != text:
            target.write_text(text, encoding='utf-8', newline='\n')
        raw_total += sum(size[0] for size in sizes.values())
        packed_total += sum(size[1] for size in sizes.values())
    return raw_total, packed_total


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    raw, packed = generate(Path(__file__).resolve().parents[1], args.output_dir)
    print(f'LVGL image payloads: {raw} -> {packed} bytes ({raw - packed} saved)')
