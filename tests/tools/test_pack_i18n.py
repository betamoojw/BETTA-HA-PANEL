# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Exact catalog bytes, deterministic generated output and bounded decoder inputs."""
import importlib.util
import json
from pathlib import Path
import re
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pack_i18n", ROOT / "tools/pack_i18n.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class PackedCatalogTests(unittest.TestCase):
    def test_all_catalogs_preserve_exact_existing_c_string_bytes(self):
        for code in pack.LANGUAGES:
            with self.subTest(language=code):
                source = (ROOT / "main/i18n" / (code + ".inc")).read_text(encoding="utf-8")
                literals = source.split("=", 1)[1].strip().removesuffix(";")
                expected = "".join(json.loads(s) for s in re.findall(r'"(?:\\.|[^"\\])*"', literals)).encode()
                data = pack.catalog_bytes(ROOT / "main/i18n" / (code + ".json"))
                self.assertEqual(data, expected)
                self.assertEqual(zlib.decompress(zlib.compress(data, 9)), expected)

    def test_generated_arrays_round_trip_and_do_not_rewrite_unchanged_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "catalogs.h"
            raw, packed = pack.generate(ROOT / "main/i18n", output)
            self.assertGreater(raw - packed, 80000)
            before = output.read_bytes(), output.stat().st_mtime_ns
            pack.generate(ROOT / "main/i18n", output)
            self.assertEqual(before, (output.read_bytes(), output.stat().st_mtime_ns))
            arrays = re.findall(r'static const unsigned char i18n_packed_([a-z_]+)\[\] = \{(.*?)\};', output.read_text(), re.S)
            self.assertEqual(len(arrays), 5)
            for symbol, body in arrays:
                data = bytes(int(token, 16) for token in re.findall(r'0x([0-9a-f]{2})', body))
                self.assertEqual(zlib.decompress(data), pack.catalog_bytes(ROOT / "main/i18n" / (symbol.replace("_", "-") + ".json")))

    def test_oversized_catalog_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "oversized.json"
            source.write_text(json.dumps({"lvgl": {"text": "a" * pack.MAX_JSON_BYTES}}))
            with self.assertRaisesRegex(ValueError, "limit"):
                pack.catalog_bytes(source)
