#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Capture/compare protected regions for the current 32 MiB P4 partition table.

Capture enters download mode and leaves the panel there. Run only as part of an
authorized flash, before and after the app write, without booting between them.
No flash writes, erases or firmware resets are performed by this helper.
"""
import argparse
import hashlib
import json
from pathlib import Path

REGIONS = {"metadata": (0, 0x20000), "coredump": (0xD20000, 0x40000),
           "storage": (0xD60000, 0x12A0000)}


def require_supported(esp, flash_id):
    if esp.CHIP_NAME != "ESP32-P4" or not esp.IS_STUB:
        raise ValueError("32 MiB verification requires the ESP32-P4 flasher stub; ROM fallback refused")
    # Conservative allowlist: W25Q256 and GD25Q256 (JEDEC manufacturer/type/capacity).
    if flash_id not in (0x1940EF, 0x1940C8):
        raise ValueError(f"Flash JEDEC ID {flash_id:06x} is not validated for this workflow")


def capture(esp, table):
    flash_id = esp.flash_id()
    require_supported(esp, flash_id)
    esp.flash_set_parameters(0x2000000)
    actual = esp.read_flash(0x8000, len(table))
    if actual != table:
        raise ValueError("Installed partition table differs from the supplied build")
    # Ensure this is the exact supported layout, not merely a matching other build.
    import struct
    partitions = {}
    for pos in range(0, len(table) - 31, 32):
        magic, typ, sub, offset, size, label, flags = struct.unpack_from("<HBBII16sI", table, pos)
        if magic == 0x50AA:
            partitions[label.rstrip(b"\0").decode()] = (offset, size)
    expected = {"nvs": (0x9000, 0x6000), "otadata": (0xF000, 0x2000),
                "phy_init": (0x11000, 0x1000), "ota_0": (0x20000, 0x680000),
                "ota_1": (0x6A0000, 0x680000), "coredump": REGIONS["coredump"],
                "storage": REGIONS["storage"]}
    if partitions != expected:
        raise ValueError("Unsupported partition layout")
    return {"schema": 1, "reader": "esp32p4-stub", "mac": list(esp.read_mac()),
            "flash_id": flash_id, "table_sha256": hashlib.sha256(table).hexdigest(),
            "regions": {name: {"offset": start, "size": size,
                               "md5": esp.flash_md5sum(start, size)}
                        for name, (start, size) in REGIONS.items()}}


def compare(before, after):
    for record in (before, after):
        if record.get("schema") != 1 or record.get("reader") != "esp32p4-stub":
            raise ValueError("Unsupported or legacy ROM checksum record")
        if set(record.get("regions", {})) != set(REGIONS):
            raise ValueError("Incomplete protected regions")
        for name, (start, size) in REGIONS.items():
            r = record["regions"][name]
            if (r.get("offset"), r.get("size")) != (start, size):
                raise ValueError("Protected region geometry changed")
            digest = r.get("md5", "")
            if len(digest) != 32 or any(c not in "0123456789abcdef" for c in digest):
                raise ValueError("Invalid digest")
    if before != after:
        raise ValueError("Device identity, partition table or protected content changed; retain evidence")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    cap = sub.add_parser("capture")
    cap.add_argument("--port", required=True)
    cap.add_argument("--table", type=Path, required=True)
    cap.add_argument("--out", type=Path, required=True)
    cmp = sub.add_parser("compare")
    cmp.add_argument("before", type=Path)
    cmp.add_argument("after", type=Path)
    args = parser.parse_args()
    if args.action == "compare":
        compare(json.loads(args.before.read_text()), json.loads(args.after.read_text()))
        print("All protected regions unchanged (before firmware boot).")
        return
    if args.out.exists():
        raise ValueError("Output exists; refusing to overwrite evidence")
    table = args.table.read_bytes()
    import esptool
    from esptool import cmds
    rom = esptool.detect_chip(port=args.port, baud=115200)
    try:
        esp = cmds.run_stub(rom)
        if not esp.IS_STUB or esp.CHIP_NAME != "ESP32-P4":
            raise ValueError("ESP32-P4 stub required")
        cmds.attach_flash(esp)
        record = capture(esp, table)
        with args.out.open("x") as output:
            json.dump(record, output, indent=2)
    finally:
        rom._port.close()
    print("Protected checksums saved; panel remains in download mode.")


if __name__ == "__main__":
    main()
