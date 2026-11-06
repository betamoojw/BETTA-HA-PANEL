# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Run production session commands/packets/worker with deterministic audio/time."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_xiaozhi_connection import function

ROOT = Path(__file__).resolve().parents[2]


class AssistantDialogPolicy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc = os.environ.get("CC", "cc")
        cls.cjson = ROOT / "managed_components/espressif__cjson/cJSON"
        cls.sr = ROOT / "managed_components/espressif__esp-sr/include/esp32p4"
        if not shutil.which(cls.cc) or not (cls.cjson / "cJSON.c").is_file() or not cls.sr.is_dir():
            raise unittest.SkipTest("Provide CC and fetched panel10 cJSON/ESP-SR components")
        cls.temp = tempfile.TemporaryDirectory(prefix="betta-assistant-dialog-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        source = (ROOT / "main/xiaozhi/xiaozhi.c").read_text(encoding="utf-8")
        cls.functions = "".join(function(source, name) for name in (
            "show_dialog", "end_dialog", "conversation", "set_state", "str",
            "receive_packet", "wake_config_result", "prepare_turn_start", "turn_start_current",
            "defer_turn_end", "take_turn_end", "handle_command", "send_capture", "worker", "enqueue_command"))
        cls.fixed = cls.compile("fixed", cls.functions)
        cls.mutants = [
            (cls.compile("tts-farewell", cls.functions.replace(
                "!tts && status.dialog_active && text[0] && xiaozhi_farewell(text)",
                "status.dialog_active && text[0] && xiaozhi_farewell(text)")), "noncommands"),
            (cls.compile("stale-open", cls.functions.replace(
                "if (status.dialog_active || (uint32_t)c->value != status.dialog_seq) return;", "")), "commands"),
            (cls.compile("stale-stop", cls.functions.replace(
                "if (!status.dialog_active || (uint32_t)c->value != status.dialog_seq) return;", "")), "commands"),
            (cls.compile("unacknowledged-error", cls.functions.replace(
                'strlcpy(status.dialog_end_reason, "manual", sizeof(status.dialog_end_reason));',
                'strlcpy(status.dialog_end_reason, "error", sizeof(status.dialog_end_reason));')), "dismiss-error"),
        ]

    @classmethod
    def compile(cls, name, source):
        unit = cls.directory / name
        unit.mkdir()
        (unit / "xiaozhi_dialog_functions.h").write_text(source, encoding="utf-8")
        exe = unit / ("test.exe" if os.name == "nt" else "test")
        args = [cls.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                "-I", str(unit), "-I", str(ROOT / "main"), "-I", str(cls.cjson), "-I", str(cls.sr),
                "-I", str(ROOT / "tests/host/xiaozhi_api_stubs"),
                "-I", str(ROOT / "tests/host/duplex_stubs"), "-I", str(ROOT / "tests/host/stubs"),
                str(ROOT / "tests/host/test_xiaozhi_dialog_policy.c"),
                str(ROOT / "main/xiaozhi/xiaozhi_activity.c"),
                str(ROOT / "main/xiaozhi/xiaozhi_farewell.c"),
                str(cls.cjson / "cJSON.c"), "-o", str(exe)]
        result = subprocess.run(args, capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return exe

    def test_production_dialog_policy(self):
        for case in ("farewell", "noncommands", "commands", "audio", "capture", "timeout", "queue", "dismiss-error"):
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
