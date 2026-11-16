# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Decode real baseline JPEG bands independently, including restart wrap/tails."""
import io
import os
import random
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


class JpegBands(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="betta-jpeg-bands-")
        cls.directory = Path(cls.tmp.name)
        cls.exe = cls.directory / ("join.exe" if os.name == "nt" else "join")
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "main"),
            str(ROOT / "tests/host/test_screenshot_jpeg_join.c"),
            str(ROOT / "main/api/screenshot_jpeg_join.c"), "-o", str(cls.exe)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    @staticmethod
    def jpeg(image, **kwargs):
        data = io.BytesIO()
        image.save(data, "JPEG", quality=90, subsampling=0, **kwargs)
        return data.getvalue()

    def join(self, width, height, bands, success=True):
        paths = []
        for i, data in enumerate(bands):
            path = self.directory / f"band-{i}.jpg"
            path.write_bytes(data)
            paths.append(str(path))
        out = self.directory / "result.jpg"
        if out.exists():
            out.unlink()
        result = subprocess.run([str(self.exe), str(width), str(height), str(out), *paths])
        self.assertEqual(result.returncode == 0, success)
        if success:
            with Image.open(out) as image:
                image.load()
                self.assertEqual(image.size, (width, height))
                return image.tobytes()
        self.assertFalse(out.exists())

    def test_pixels_and_geometry(self):
        for w, h in [(3, 2), (13, 67), (480, 480), (1280, 800), (800, 1280)]:
            with self.subTest(size=(w, h)):
                raw = bytes(((x // 16) * 13 + (y // 13) * 7 + c * 63) & 255
                            for y in range(h) for x in range(w) for c in range(3))
                image = Image.frombytes("RGB", (w, h), raw)
                bands = [self.jpeg(image.crop((0, y, w, min(h, y + 32)))) for y in range(0, h, 32)]
                actual = self.join(w, h, bands)
                with Image.open(io.BytesIO(self.jpeg(image))) as expected:
                    self.assertEqual(actual, expected.tobytes())

    def test_output_limit(self):
        image = Image.frombytes("RGB", (1280, 800), random.Random(2026).randbytes(1280 * 800 * 3))
        bands = [self.jpeg(image.crop((0, y, 1280, y + 32))) for y in range(0, 800, 32)]
        self.assertGreater(sum(map(len, bands)), 1 << 20)
        self.join(1280, 800, bands, success=False)

    def test_reject_incompatible_or_incomplete_bands(self):
        image = Image.new("RGB", (16, 32), (123, 14, 210))
        first = self.jpeg(image)
        variants = [first[:-1], first + b"x", self.jpeg(image, progressive=True),
                    self.jpeg(Image.new("RGB", (17, 32))), self.jpeg(Image.new("RGB", (16, 31)))]
        changed = bytearray(first)
        changed[changed.index(b"\xff\xdb") + 5] ^= 1
        variants.append(bytes(changed))
        for bad in variants:
            with self.subTest(prefix=bad[:8], length=len(bad)):
                self.join(16, 64, [first, bad], success=False)
        self.join(16, 64, [first], success=False)
        self.join(16, 32, [first, first], success=False)
        self.join(0, 32, [first], success=False)


if __name__ == "__main__":
    unittest.main()
