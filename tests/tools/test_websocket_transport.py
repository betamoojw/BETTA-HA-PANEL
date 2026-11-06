# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Run the actual fetched WebSocket send/receive functions with transport faults."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("ws_patch", ROOT / "tools/patch_websocket_transport.py")
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)


def function(source, name):
    start = source.index("static ", source.index(name) - 20)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


class WebSocketTransport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = ROOT / "managed_components/espressif__esp_websocket_client/esp_websocket_client.c"
        cls.cc = os.environ.get("CC", "cc")
        if not source.exists() or not shutil.which(cls.cc):
            raise unittest.SkipTest("Fetch esp_websocket_client and provide a host C compiler (CC)")
        cls.temp = tempfile.TemporaryDirectory(prefix="betta-ws-regression-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.original = source.read_text(encoding="utf-8")
        # Permit a fresh fetched component as well as the current patched tree.
        if patch.EDITS[-1][1] in cls.original:
            for old, new in reversed(patch.EDITS):
                if new:
                    cls.original = cls.original.replace(new, old, 1)
            cls.original = cls.original.replace(
                "    while (client->run) {", "    int read_select = 0;\n    while (client->run) {", 1)
        pristine = cls.directory / "pristine.c"
        pristine.write_text(cls.original, encoding="utf-8")
        state, cls.patched = patch.patch_websocket_transport(pristine)
        if state != "applied":
            raise AssertionError("Patch does not match the fetched dependency")
        cls.fixed = cls.compile(cls.patched, "fixed")
        cls.before = cls.compile(cls.original, "before")
        cls.unsafe = cls.compile(cls.original, "unsafe", separate=True)

    @classmethod
    def compile(cls, source, name, separate=False):
        unit = cls.directory / name
        unit.mkdir()
        functions = function(source, "esp_websocket_client_send_with_exact_opcode(")
        functions += function(source, "esp_websocket_client_recv(")
        functions += function(cls.patched, "esp_websocket_client_poll_connection(")
        (unit / "websocket_functions.h").write_text(functions, encoding="utf-8")
        exe = unit / ("test.exe" if os.name == "nt" else "test")
        # The upstream TickType_t-to-millisecond ternary mixes signedness.
        args = [cls.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", "-I", str(unit)]
        if separate:
            args.append("-DCONFIG_ESP_WS_CLIENT_SEPARATE_TX_LOCK=1")
        if os.name == "nt":
            args.append("-D_WIN32_WINNT=0x0600")
        args += [str(ROOT / "tests/host/test_websocket_transport.c"), "-o", str(exe)]
        result = subprocess.run(args, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return exe

    def execute(self, executable, case):
        return subprocess.run([str(executable), case], capture_output=True, text=True, timeout=5)

    def test_faults_and_duplex(self):
        for case in ["concurrent", "closed_while_waiting", "send_timeout", "write_failure",
                     "partial_timeout", "trickle_deadline", "stopped_receive", "empty_control",
                     "poll_recovery", "idle_and_duplex", "fragmented"]:
            with self.subTest(case=case):
                result = self.execute(self.fixed, case)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_negative_controls_reproduce_original_failures(self):
        for exe, case in [(self.unsafe, "concurrent"), (self.before, "closed_while_waiting"),
                          (self.before, "partial_timeout"), (self.before, "trickle_deadline")]:
            with self.subTest(case=case):
                self.assertEqual(self.execute(exe, case).returncode, 99)

    def test_patch_is_idempotent_and_rejects_unknown_source(self):
        target = self.directory / "patch-check.c"
        target.write_text(self.patched, encoding="utf-8")
        self.assertEqual(patch.patch_websocket_transport(target), ("present", None))
        target.write_text(self.original.replace("    int read_select = 0;", "    int changed = 0;"), encoding="utf-8")
        self.assertEqual(patch.patch_websocket_transport(target), ("missing", None))


if __name__ == "__main__":
    unittest.main()
