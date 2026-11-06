# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""Compile and exercise the production assistant transmit outbox on the host."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class AssistantTransmit(unittest.TestCase):
    def test_outbox(self):
        cc = os.environ.get("CC", "cc")
        if not shutil.which(cc):
            self.skipTest("Provide a host C compiler (CC)")
        with tempfile.TemporaryDirectory(prefix="betta-assistant-tx-") as directory:
            exe = Path(directory) / ("test.exe" if os.name == "nt" else "test")
            result = subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                                     "-I", str(ROOT / "main"), "-I", str(ROOT / "tests/host/duplex_stubs"),
                                     str(ROOT / "tests/host/test_xiaozhi_tx.c"), "-o", str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
