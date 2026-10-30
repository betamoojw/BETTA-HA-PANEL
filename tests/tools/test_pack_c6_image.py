# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
import hashlib
from pathlib import Path
import struct
import sys
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from pack_c6_image import pack


class C6PackingTest(unittest.TestCase):
    def image(self):
        data = bytearray(1024)
        data[0] = 0xE9
        struct.pack_into("<H", data, 12, 13)
        struct.pack_into("<I", data, 32, 0xABCD5432)
        return data

    def test_deterministic_verified_round_trip(self):
        data = self.image()
        archive = pack(data)
        self.assertEqual(archive, pack(data))
        self.assertEqual(archive[:4], b"BC6Z")
        self.assertEqual(struct.unpack_from("<I", archive, 4)[0], len(data))
        self.assertEqual(archive[8:40], hashlib.sha256(data).digest())
        self.assertEqual(zlib.decompress(archive[40:]), data)

    def test_wrong_chip_header_and_descriptor_are_rejected(self):
        for offset in (0, 12, 32):
            data = self.image()
            data[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                pack(data)

    def test_truncated_and_oversize_images_are_rejected(self):
        for data in (self.image()[:287], self.image() + bytearray(4 * 1024 * 1024)):
            with self.assertRaises(ValueError):
                pack(data)
