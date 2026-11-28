# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
from pathlib import Path
import sys
import unittest

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import generate_brand_logo as logo


class BrandLogoTests(unittest.TestCase):
    def test_rgb565_rounding_byte_order_and_expansion(self):
        pixels = Image.new("RGB", (5, 1))
        pixels.putdata([(0, 0, 0), (255, 0, 0), (0, 255, 0), (3, 1, 4), (255, 255, 255)])
        packed, expanded = logo.color_data(pixels)
        self.assertEqual(packed, bytes.fromhex("0000 00f8 e007 0100 ffff"))
        self.assertEqual(expanded, bytes.fromhex("000000 ff0000 00ff00 000008 ffffff"))

    def test_firmware_png_and_simulator_pixels_match_the_master(self):
        logo.generate(check=True)


if __name__ == "__main__":
    unittest.main()
