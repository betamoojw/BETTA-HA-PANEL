# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Byte-identical CJK packing, generation stability and production decoder faults."""
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pack_cjk_font", ROOT / "tools/pack_cjk_font.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)
SOURCE = ROOT / "main/ui/fonts/noto_cjk_subset.ttf"


class PackedCjkFontTests(unittest.TestCase):
    def test_original_font_and_c_array_are_identical(self):
        source = (ROOT / "main/ui/fonts/ttf_cjk_data.c").read_text(encoding="utf-8")
        body = source.split("ttf_cjk_data[] = {", 1)[1].split("}", 1)[0]
        self.assertEqual(bytes(map(int, re.findall(r"\d+", body))), SOURCE.read_bytes())

    def test_generated_header_round_trips_without_rewriting(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "cjk_font_packed.h"
            raw, packed = pack.generate(SOURCE, output)
            self.assertGreater(raw - packed, 90000)
            before = output.read_bytes(), output.stat().st_mtime_ns
            self.assertEqual(pack.generate(SOURCE, output), (raw, packed))
            self.assertEqual(before, (output.read_bytes(), output.stat().st_mtime_ns))
            compressed = bytes(int(byte, 16) for byte in re.findall(r"0x([0-9a-f]{2})", output.read_text()))
            self.assertEqual(len(compressed), packed)
            self.assertEqual(zlib.decompress(compressed), SOURCE.read_bytes())

    def test_invalid_fonts_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, output = Path(tmp) / "bad.ttf", Path(tmp) / "packed.h"
            for data in (b"", b"\0\1\0\0", b"bad!" + bytes(12), b"\0\1\0\0" + bytes(pack.MAX_FONT_BYTES)):
                with self.subTest(size=len(data)):
                    source.write_bytes(data)
                    with self.assertRaisesRegex(ValueError, "bounded TrueType"):
                        pack.generate(source, output)
                    self.assertFalse(output.exists())

    def test_production_decoder_and_raw_board_accessor(self):
        cc = os.environ.get("CC", "cc")
        if not shutil.which(cc):
            if os.name != "nt" or os.environ.get("CI"):
                self.fail("Production CJK decoder coverage requires a host C compiler; set CC")
            self.skipTest("Provide a host C compiler through CC")
        zlib_dir = ROOT / "managed_components/espressif__zlib/zlib"
        zlib_sources, zlib_flags, zlib_libraries = [], [], []
        if (zlib_dir / "inflate.c").exists():
            zlib_sources = [zlib_dir / (name + ".c") for name in
                            ("adler32", "crc32", "compress", "deflate", "trees", "uncompr", "inflate", "inffast", "inftrees", "zutil")]
            zlib_flags = ["-I", str(zlib_dir)]
        else:
            # Fresh CI checks use the system development package, without an
            # ESP-IDF dependency fetch. Missing headers/library must fail below.
            zlib_libraries = ["-lz"]
        with tempfile.TemporaryDirectory(prefix="betta-cjk-font-") as tmp:
            directory = Path(tmp)
            pack.generate(SOURCE, directory / "cjk_font_packed.h")
            suffix = ".exe" if os.name == "nt" else ""
            executable = directory / ("decoder" + suffix)
            sources = [ROOT / "tests/host/test_cjk_font_data.c", ROOT / "main/ui/fonts/ttf_cjk_data.c", *zlib_sources]
            command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-DAPP_CJK_COMPRESSED=1",
                       "-I", str(ROOT / "main"), "-I", str(ROOT / "tests/host/c6_image_stubs"),
                       "-I", str(directory), *zlib_flags, *map(str, sources), *zlib_libraries, "-o", str(executable)]
            build = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, "CJK decoder requires zlib development headers/library or the fetched component:\n" + build.stdout + build.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("checks passed", result.stdout)
            print(result.stdout.strip())
            # Non-Panel10 builds and the simulator keep their direct read-only data path.
            raw_source = directory / "raw.c"
            raw_source.write_text('#include <assert.h>\n#include "ui/cjk_font_data.h"\n'
                                  'int main(void) { unsigned int n=0; assert(ui_cjk_font_data(&n)==ttf_cjk_data); '
                                  'assert(n==ttf_cjk_size); return 0; }\n', encoding="utf-8")
            raw_executable = directory / ("raw" + suffix)
            build = subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "main"),
                                    str(raw_source), str(ROOT / "main/ui/fonts/ttf_cjk_data.c"), "-o", str(raw_executable)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            self.assertEqual(subprocess.run([str(raw_executable)], timeout=5).returncode, 0)


if __name__ == "__main__":
    unittest.main()
