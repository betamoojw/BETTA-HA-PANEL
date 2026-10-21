# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
import copy
import importlib.util
from pathlib import Path
import unittest
import struct

spec = importlib.util.spec_from_file_location("check", Path(__file__).parents[1] / "tools/flash_storage_check.py")
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


class StorageChecks(unittest.TestCase):
    def record(self):
        return {"schema": 1, "reader": "esp32p4-stub", "mac": [1, 2, 3, 4, 5, 6],
                "flash_id": 0x1940EF, "table_sha256": "a" * 64,
                "regions": {n: {"offset": a, "size": s, "md5": "a" * 32}
                            for n, (a, s) in check.REGIONS.items()}}

    def test_unchanged(self):
        check.compare(self.record(), self.record())

    def test_changed_upper_storage(self):
        a = self.record(); b = copy.deepcopy(a)
        b["regions"]["storage"]["md5"] = "b" * 32
        with self.assertRaises(ValueError): check.compare(a, b)

    def test_other_device(self):
        a = self.record(); b = copy.deepcopy(a); b["mac"][0] = 2
        with self.assertRaises(ValueError): check.compare(a, b)

    def test_legacy_rom(self):
        with self.assertRaises(ValueError): check.compare({"storage": "a" * 32}, self.record())

    def test_incomplete(self):
        a = self.record(); del a["regions"]["storage"]
        with self.assertRaises(ValueError): check.compare(a, a)

    def test_rom_refused(self):
        class ROM:
            CHIP_NAME = "ESP32-P4"
            IS_STUB = False
        with self.assertRaises(ValueError): check.require_supported(ROM(), 0x1940EF)

    def test_supported_flash(self):
        class Stub:
            CHIP_NAME = "ESP32-P4"
            IS_STUB = True
        check.require_supported(Stub(), 0x1940EF)
        check.require_supported(Stub(), 0x1940C8)
        with self.assertRaises(ValueError): check.require_supported(Stub(), 0x1840EF)

    def test_capture_full_storage_with_stub(self):
        parts = {"nvs": (0x9000, 0x6000), "otadata": (0xF000, 0x2000),
                 "phy_init": (0x11000, 0x1000), "ota_0": (0x20000, 0x680000),
                 "ota_1": (0x6A0000, 0x680000), "coredump": (0xD20000, 0x40000),
                 "storage": (0xD60000, 0x12A0000)}
        table = b"".join(struct.pack("<HBBII16sI", 0x50AA, 1, 0, a, s, n.encode(), 0)
                         for n, (a, s) in parts.items())
        class Stub:
            CHIP_NAME = "ESP32-P4"
            IS_STUB = True
            calls = []
            def flash_id(self): return 0x1940EF
            def flash_set_parameters(self, size): self.size = size
            def read_flash(self, start, size): return table
            def read_mac(self): return (1, 2, 3, 4, 5, 6)
            def flash_md5sum(self, start, size):
                self.calls.append((start, size))
                return "a" * 32
        stub = Stub()
        result = check.capture(stub, table)
        self.assertEqual(stub.size, 0x2000000)
        self.assertIn((0xD60000, 0x12A0000), stub.calls)
        check.compare(result, result)
        with self.assertRaises(ValueError): check.capture(stub, table[:-32])


if __name__ == "__main__":
    unittest.main()
