#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Verify and embed a single Panel10 CJK subset."""
import argparse
from pathlib import Path
import sys

# This legacy entry point also supports callers loading the standalone file by path.
tool_dir = str(Path(__file__).resolve().parent)
if tool_dir not in sys.path:
    sys.path.insert(0, tool_dir)
from pack_ui_fonts import MAX_FONT_BYTES, generate

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--packed-source", type=Path, required=True)
    args = parser.parse_args()
    raw, packed = generate(args.source, args.output, args.packed_source)
    print(f"Panel10 CJK subset: {raw} -> {packed} bytes ({raw - packed} saved before code/alignment)")
