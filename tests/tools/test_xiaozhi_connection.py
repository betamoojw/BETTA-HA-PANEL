# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Exercise production Xiaozhi callbacks, send races and worker close handling."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    start = re.search(r"^static [^\n]+\b" + name + r"\(", source, re.MULTILINE).start()
    opening = source.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


class XiaozhiConnection(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc = os.environ.get("CC", "cc")
        if not shutil.which(cls.cc):
            raise unittest.SkipTest("Provide a host C compiler (CC)")
        cls.temp = tempfile.TemporaryDirectory(prefix="betta-xiaozhi-close-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        source = (ROOT / "main/xiaozhi/xiaozhi.c").read_text(encoding="utf-8")
        cls.functions = "".join(function(source, name) for name in (
            "connection_snapshot", "connection_fail", "connection_can_send", "connection_reset",
            "send_packet", "record_connection_end", "disconnect_session", "ws_event", "finish_connection"))
        cls.fixed = cls.compile("fixed", cls.functions)
        # Prove coverage of the ignored CLOSE, stale event code and final callback races.
        cls.mutants = [
            (cls.compile("ignore-close", cls.functions.replace(
                "esp_websocket_event_data_t *e = data;",
                "esp_websocket_event_data_t *e = data; if (e && id == WEBSOCKET_EVENT_DATA && e->op_code == 8) return;")), "queued-close"),
            (cls.compile("stale-code", cls.functions.replace(
                "xiaozhi_connection_close_frame(&connection, e->data_ptr, e->data_len, e->payload_len, e->payload_offset, e->fin);",
                "xiaozhi_connection_close_code(&connection, e->close_status_code);")), "abnormal"),
            (cls.compile("early-snapshot", cls.functions.replace(
                "link = connection_snapshot(); /* stop/destroy joined the final callback */", "")), "late-error"),
            (cls.compile("early-diagnosis", cls.functions.replace(
                "if (had_connection) record_connection_end();", "").replace(
                "bool had_connection = ws != NULL;", "if (ws) record_connection_end();")), "local-stop"),
        ]

    @classmethod
    def compile(cls, name, source):
        unit = cls.directory / name
        unit.mkdir()
        (unit / "xiaozhi_connection_functions.h").write_text(source, encoding="utf-8")
        exe = unit / ("test.exe" if os.name == "nt" else "test")
        args = [cls.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                "-I", str(unit), "-I", str(ROOT / "main"),
                "-I", str(ROOT / "tests/host/xiaozhi_api_stubs"),
                "-I", str(ROOT / "tests/host/stubs"),
                str(ROOT / "tests/host/test_xiaozhi_connection.c"), "-o", str(exe)]
        result = subprocess.run(args, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return exe

    def test_close_faults_and_rewake(self):
        for case in ("normal", "queued-close", "abnormal", "malformed", "transport",
                     "late-error", "timeout", "unexpected-finish", "rewake", "local-stop"):
            with self.subTest(case=case):
                result = subprocess.run([str(self.fixed), case], capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_negative_controls(self):
        for exe, case in self.mutants:
            with self.subTest(case=case):
                result = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 99, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
