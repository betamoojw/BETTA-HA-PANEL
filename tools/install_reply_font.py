#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Check/install only the pinned local reply font. Never edit settings or reset a panel."""
import argparse
import base64
import json
import os
import time
import urllib.parse
import urllib.request
from prepare_reply_font import verified_resource


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--url", required=True, help="Panel origin, e.g. http://192.168.1.10")
    p.add_argument("--install", action="store_true", help="Explicitly install after checking identity and space")
    p.add_argument("--username", help="Optional editor Basic-auth user; password from BETTA_EDITOR_PASSWORD")
    args = p.parse_args()
    origin = args.url.rstrip("/"); parsed = urllib.parse.urlsplit(origin)
    if parsed.scheme not in ("http", "https") or not parsed.netloc or parsed.username or parsed.path or parsed.query or parsed.fragment:
        p.error("Use a plain panel origin without credentials, path or query")
    raw, meta = verified_resource()
    headers = {"Origin": origin, "Connection": "close"}
    if args.username:
        password = os.environ.get("BETTA_EDITOR_PASSWORD")
        if password is None: p.error("Set BETTA_EDITOR_PASSWORD; it is never printed or stored")
        headers["Authorization"] = "Basic " + base64.b64encode(f"{args.username}:{password}".encode()).decode()

    def request(chunk=None, offset=0):
        h = dict(headers)
        if chunk is not None: h.update({"Content-Type": "application/octet-stream", "X-Font-Offset": str(offset)})
        req = urllib.request.Request(origin + "/api/xiaozhi/font", data=chunk, headers=h, method="GET" if chunk is None else "PUT")
        with urllib.request.urlopen(req, timeout=60) as response: return json.load(response)

    status = request()
    if status.get("sha256") != meta["sha256"] or status.get("bytes") != len(raw):
        raise ValueError("Panel firmware and font resource do not match")
    print("Verified font already installed" if status["ready"] else "Font missing; settings untouched")
    if status["ready"] or not args.install: return
    # Restart our private .part resource after interruption. A verified resource
    # is immutable, and no user-selected path is ever sent to the device.
    for offset in range(0, len(raw), 32768):
        chunk = raw[offset:offset + 32768]
        try:
            status = request(chunk, offset)
        except (OSError, ValueError):
            time.sleep(1); status = request()
            if not status["ready"] and status["received"] != offset + len(chunk):
                raise RuntimeError("Upload interrupted; rerun to restart the temporary resource") from None
        if (offset // 32768) % 32 == 0: print(f'Uploaded {offset + len(chunk)}/{len(raw)} bytes', flush=True)
        time.sleep(.15)  # respect the device's per-client request limit
    status = request()
    if not status["ready"]: raise RuntimeError("Device did not verify the resource")
    print("Font installed and SHA256 verified; no settings changed, no reboot needed")


if __name__ == "__main__":
    main()
