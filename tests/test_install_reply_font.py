# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import install_reply_font as installer


class InstallerTests(unittest.TestCase):
    def run_install(self, install=False, ready=False, bad_identity=False, lost_reply=False):
        raw = b"a" * 65540
        sha = hashlib.sha256(raw).hexdigest()
        state = dict(ready=ready, sha256="wrong" if bad_identity else sha, bytes=len(raw), received=0)
        requests, received = [], bytearray()

        def open_request(req, timeout):
            self.assertEqual(req.full_url, "http://panel.test/api/xiaozhi/font")
            self.assertEqual(req.get_header("Origin"), "http://panel.test")
            self.assertEqual(timeout, 60)
            requests.append(req.get_method())
            if req.get_method() == "PUT":
                self.assertEqual(req.get_header("Content-type"), "application/octet-stream")
                self.assertEqual(int(req.get_header("X-font-offset")), len(received))
                self.assertLessEqual(len(req.data), 32768)
                received.extend(req.data); state["received"] = len(received)
                state["ready"] = len(received) == len(raw)
                if lost_reply and len(received) == 32768: raise OSError("lost reply after successful write")
            return io.BytesIO(json.dumps(state).encode())

        argv = ["installer", "--url", "http://panel.test"] + (["--install"] if install else [])
        with patch.object(sys, "argv", argv), patch.object(installer, "verified_resource", return_value=(raw, {"sha256": sha})), \
             patch.object(installer.urllib.request, "urlopen", side_effect=open_request), patch.object(installer.time, "sleep"), \
             contextlib.redirect_stdout(io.StringIO()):
            if bad_identity:
                with self.assertRaises(ValueError): installer.main()
            else: installer.main()
        return requests, bytes(received), raw

    def test_default_is_read_only(self):
        calls, data, _ = self.run_install(); self.assertEqual(calls, ["GET"]); self.assertFalse(data)

    def test_existing_verified_resource_is_not_written(self):
        calls, data, _ = self.run_install(True, ready=True); self.assertEqual(calls, ["GET"]); self.assertFalse(data)

    def test_chunks_preserve_exact_bytes_and_final_verification(self):
        calls, data, raw = self.run_install(True); self.assertEqual(data, raw); self.assertEqual(calls.count("PUT"), 3)
        self.assertEqual(calls[-1], "GET")

    def test_lost_reply_does_not_duplicate_successful_chunk(self):
        calls, data, raw = self.run_install(True, lost_reply=True); self.assertEqual(data, raw)
        self.assertEqual(calls.count("PUT"), 3)

    def test_wrong_firmware_resource_is_rejected_before_writing(self):
        calls, data, _ = self.run_install(True, bad_identity=True); self.assertEqual(calls, ["GET"]); self.assertFalse(data)


if __name__ == "__main__":
    unittest.main()
