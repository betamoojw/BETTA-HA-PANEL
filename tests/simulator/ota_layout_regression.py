# SPDX-License-Identifier: LicenseRef-FNCL-1.1
# Copyright (c) 2026 andre68723
"""Check the real OTA overlay against the active simulator's screen bounds."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def ota_labels(language):
    if language == "en":
        return "Firmware update", "Receiving firmware..."
    if language == "de":
        return "Firmware-Update", "Firmware wird empfangen ..."
    catalog = Path(__file__).resolve().parents[2] / "main" / "i18n" / f"{language}.json"
    ota = json.loads(catalog.read_text(encoding="utf-8"))["lvgl"]["ota"]
    return ota["title"], ota["receiving"]


def check_overlay(simulator, variant, language, step):
    with tempfile.TemporaryDirectory() as tmp:
        fixture = Path(tmp, "progress.json")
        layout = Path(tmp, "layout.jsonl")
        fixture.write_text(json.dumps({
            "pages": [{"id": "test", "title": "Test", "widgets": []}],
            "open_popup": "ota",
        }), encoding="utf-8")
        subprocess.run([
            simulator, "--fixture", str(fixture), "--page", "test", "--language", language,
            "--text-step", str(step),
            "--dump-layout", str(layout), "--exit-after-ms", "150",
        ], env={**os.environ, "SDL_VIDEODRIVER": "dummy"}, check=True)
        rows = [json.loads(line) for line in layout.read_text(encoding="utf-8").splitlines()]

    overlay = [r for r in rows if r.get("widget") == "popup"]
    screen = next(r for r in overlay if r["kind"] == "card")
    if variant == "panels3":
        assert (screen["x1"], screen["y1"], screen["x2"], screen["y2"]) == (0, 0, 479, 479), screen
    children = [r for r in overlay if r["depth"] == 2]
    assert [r.get("text", r["kind"]) for r in children] == [
        "obj", *ota_labels(language), "bar", "50 %", "2.0 MB / 4.0 MB",
    ], children
    for item in children:
        assert screen["x1"] <= item["x1"] <= item["x2"] <= screen["x2"], item
        assert screen["y1"] <= item["y1"] <= item["y2"] <= screen["y2"], item
        assert not item.get("text_clipped"), item
    for above, below in zip(children, children[1:]):
        assert above["y2"] < below["y1"], (above, below)


if __name__ == "__main__":
    simulator, variant = sys.argv[1:]
    languages = ("en", "de", "es", "fr", "pl", "zh-cn", "zh-tw") if variant == "panels3" else ("en",)
    for language in languages:
        for step in range(3 if variant == "panels3" else 1):
            try:
                check_overlay(simulator, variant, language, step)
            except AssertionError as exc:
                raise AssertionError(f"{variant}, {language}, text step {step}: {exc}") from exc
