# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Encoder boundary checks; the native LVGL suite independently checks decoding/rendering."""
from pathlib import Path
import random
import re
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from pack_lvgl_images import ARRAY, SOURCES, lz4_block, pack_source


def decode(block):
    """Strict small reference reader, also asserting LZ4's final sequence rules."""
    result = bytearray()
    pos = 0
    last_match = None
    while pos < len(block):
        token = block[pos]; pos += 1
        def length(initial):
            nonlocal pos
            value = initial
            if initial == 15:
                while True:
                    extra = block[pos]; pos += 1; value += extra
                    if extra != 255: break
            return value
        literal = length(token >> 4)
        result.extend(block[pos:pos + literal]); pos += literal
        if pos == len(block):
            assert token & 15 == 0
            assert last_match is None or (literal >= 5 and len(result) - last_match >= 12)
            return bytes(result)
        offset = struct.unpack_from('<H', block, pos)[0]; pos += 2
        assert 0 < offset <= min(65535, len(result))
        last_match = len(result)
        for _ in range(length(token & 15) + 4): result.append(result[-offset])
    raise AssertionError('Missing final literal sequence')


class PackedImagesTests(unittest.TestCase):
    def test_short_literals_overlaps_long_lengths_and_window(self):
        rng = random.Random(42)
        unique = rng.randbytes(65536)
        cases = [rng.randbytes(n) for n in (0, 1, 4, 5, 12, 13, 15, 16, 270, 65536)]
        cases += [b'a' * 100000, b'ab' * 40000, unique + unique, unique + unique[-65535:] + b'ending']
        for data in cases:
            with self.subTest(size=len(data)):
                self.assertEqual(decode(lz4_block(data)), data)
                self.assertEqual(lz4_block(data), lz4_block(data))

    def test_all_maintained_pixels_and_descriptors_are_retained(self):
        raw_total = packed_total = count = 0
        for name in SOURCES:
            source = (ROOT / 'main' / name).read_text(encoding='utf-8')
            packed, sizes = pack_source(source)
            original = {m[2]: bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', m[5]))
                        for m in ARRAY.finditer(source)}
            for match in ARRAY.finditer(packed):
                data = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', match[5]))
                if match[2] in sizes:
                    method, compressed, expanded = struct.unpack_from('<III', data)
                    self.assertEqual((method, compressed, expanded), (2, len(data)-12, len(original[match[2]])))
                    self.assertEqual(decode(data[12:]), original[match[2]])
                    raw_total += expanded; packed_total += len(data); count += 1
                else:
                    self.assertEqual(data, original[match[2]])
        self.assertEqual(count, 151)
        self.assertGreater(raw_total - packed_total, 250000)
        # The merged Panel10 feature set must still fit its unchanged OTA slots.
        self.assertLessEqual(packed_total, 171000)

    def test_unknown_source_shape_fails_closed(self):
        for text in ('', 'uint8_t a[4] = {0x00};', 'uint8_t a[] = {SYMBOL};',
                     'uint8_t a[] = {' + '0x00,' * 100 + '};'):
            with self.assertRaises(ValueError): pack_source(text)

    def test_existing_header_flags_are_not_silently_replaced(self):
        pixels = 'uint8_t pixels[] = {' + '0x00,' * 100 + '};\n'
        for header in ('.header.flags = LV_IMAGE_FLAGS_PREMULTIPLIED,',
                       '.header = {.flags = LV_IMAGE_FLAGS_PREMULTIPLIED},',
                       '.header.flags = 0,'):
            source = pixels + 'const lv_image_dsc_t image = {' + header + '.data_size = 100, .data = pixels};'
            with self.subTest(header=header):
                with self.assertRaisesRegex(ValueError, 'Explicit image flags'):
                    pack_source(source)
