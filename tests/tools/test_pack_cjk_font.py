# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Byte-identical UI fonts, generation stability and production decoder faults."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import pack_ui_fonts as pack

FONT_DIR = ROOT / "main/ui/fonts"
SOURCE = FONT_DIR / "noto_cjk_subset.ttf"
PACKED = SOURCE.with_suffix(".ttf.zlib")


class PackedFontTests(unittest.TestCase):
    def test_standalone_cjk_entry_point(self):
        legacy = ROOT / "tools/pack_cjk_font.py"
        with tempfile.TemporaryDirectory() as tmp:
            header = Path(tmp) / "cjk_font_packed.h"
            subprocess.run([sys.executable, "-I", "-B", str(legacy), str(SOURCE), str(header),
                            "--packed-source", str(PACKED)], cwd=tmp, check=True, capture_output=True)
            before = header.read_bytes()
            subprocess.run([sys.executable, "-I", "-B", "-c",
                            "import runpy, sys\nfrom pathlib import Path\n"
                            "module = runpy.run_path(sys.argv[1])\n"
                            "raw, packed = module['generate'](*map(Path, sys.argv[2:]))\n"
                            "assert packed < raw <= module['MAX_FONT_BYTES']",
                            str(legacy), str(SOURCE), str(header), str(PACKED)], cwd=tmp, check=True)
            self.assertEqual(header.read_bytes(), before)

    def test_original_font_and_c_array_are_identical(self):
        for face, meta in pack.FONTS.items():
            with self.subTest(face=face):
                source = (FONT_DIR / meta["data"]).read_text(encoding="utf-8")
                body = source.split(meta["symbol"] + "_data[] = {", 1)[1].split("}", 1)[0]
                self.assertEqual(bytes(map(int, re.findall(r"\d+", body))), (FONT_DIR / meta["subset"]).read_bytes())

    def test_generated_header_round_trips_without_rewriting(self):
        with tempfile.TemporaryDirectory() as tmp:
            pack.generate_all(FONT_DIR, Path(tmp))
            for face, meta in pack.FONTS.items():
                with self.subTest(face=face):
                    output = Path(tmp) / f"{face}_font_packed.h"
                    source = FONT_DIR / meta["subset"]
                    packed = source.with_suffix(".ttf.zlib")
                    before = output.read_bytes(), output.stat().st_mtime_ns
                    pack.generate(source, output, packed, face)
                    self.assertEqual(before, (output.read_bytes(), output.stat().st_mtime_ns))
                    header = before[0].decode()
                    self.assertIn(meta["license"], header)
                    compressed = bytes.fromhex("".join(re.findall(r"0x([0-9a-f]{2})", header)))
                    self.assertEqual(compressed, packed.read_bytes())
                    self.assertEqual(zlib.decompress(compressed), source.read_bytes())
                    self.assertLess(len(compressed), len(zlib.compress(source.read_bytes(), 9)))

    def test_invalid_or_stale_packed_fonts_are_rejected(self):
        original = PACKED.read_bytes()
        raw = SOURCE.read_bytes()
        cases = (b"", b"invalid", original[:-1], original + b"\0",
                 zlib.compress(raw[:-1]), zlib.compress(raw + b"\0"),
                 zlib.compress(raw[:-1] + bytes([raw[-1] ^ 1])))
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            output = directory / "packed.h"
            packed = directory / "font.zlib"
            for index, data in enumerate(cases):
                with self.subTest(case=index):
                    packed.write_bytes(data)
                    with self.assertRaisesRegex(ValueError, "does not match the source TTF"):
                        pack.generate(SOURCE, output, packed)
                    self.assertFalse(output.exists())

    def test_invalid_fonts_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            source, output = Path(tmp) / "bad.ttf", Path(tmp) / "packed.h"
            for data in (b"", b"\0\1\0\0", b"bad!" + bytes(12), b"\0\1\0\0" + bytes(pack.MAX_FONT_BYTES)):
                with self.subTest(size=len(data)):
                    source.write_bytes(data)
                    with self.assertRaisesRegex(ValueError, "bounded TrueType"):
                        pack.generate(source, output, PACKED)
                    self.assertFalse(output.exists())

    def test_production_decoder_and_raw_board_accessor(self):
        cc = os.environ.get("CC", "cc")
        if not shutil.which(cc):
            if os.name != "nt" or os.environ.get("CI"):
                self.fail("Production font decoder coverage requires a host C compiler; set CC")
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
            pack.generate_all(FONT_DIR, directory)
            suffix = ".exe" if os.name == "nt" else ""
            executable = directory / ("decoder" + suffix)
            arrays = [str(FONT_DIR / meta["data"]) for meta in pack.FONTS.values()]
            sources = [ROOT / "tests/host/test_cjk_font_data.c", *arrays, *zlib_sources]
            command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-DAPP_TTF_COMPRESSED=1",
                       "-I", str(ROOT / "main"), "-I", str(ROOT / "tests/host/c6_image_stubs"),
                       "-I", str(directory), *zlib_flags, *map(str, sources), *zlib_libraries, "-o", str(executable)]
            build = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, "Font decoder requires zlib development headers/library or the fetched component:\n" + build.stdout + build.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("checks passed", result.stdout)
            print(result.stdout.strip())
            # Non-Panel10 builds and the simulator keep their direct read-only data path.
            raw_source = directory / "raw.c"
            raw_checks = [
                f'assert(ui_font_data(UI_TTF_{face.upper()}, &n) == {meta["symbol"]}_data && n == {meta["symbol"]}_size);'
                for face, meta in pack.FONTS.items()
            ]
            raw_source.write_text('#include <assert.h>\n#include "ui/font_data.h"\nint main(void) { unsigned int n;\n'
                                  + "\n".join(raw_checks) + '\nreturn 0; }\n', encoding="utf-8")
            raw_executable = directory / ("raw" + suffix)
            build = subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "main"),
                                    str(raw_source), *arrays, "-o", str(raw_executable)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            self.assertEqual(subprocess.run([str(raw_executable)], timeout=5).returncode, 0)


if __name__ == "__main__":
    unittest.main()
