# SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723
"""A saved forecast tile must render exactly like its current-format equivalent."""
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from generate_simulator_gallery import use_fixed_clock

use_fixed_clock()
fixture = {"version": 1, "pages": [{"id": "weather", "widgets": [{
    "id": "legacy_forecast", "type": "weather_3day", "entity_id": "weather.demo",
    "forecast_days": 3, "rect": {"x": 12, "y": 12, "w": 320, "h": 300}}]}],
    "states": [{"entity_id": "weather.demo", "state": "sunny", "attributes": {
        "temperature": 21, "temperature_unit": "°C", "humidity": 50, "precipitation_unit": "mm",
        "forecast": [{"date_offset_days": day, "condition": "sunny", "temperature": 23 + day,
                      "templow": 12 + day, "precipitation": 0} for day in range(4)]}}]}
current = copy.deepcopy(fixture)
current["pages"][0]["widgets"][0].update(type="weather_tile", weather_view="days")
env = dict(os.environ, SDL_VIDEODRIVER="dummy")
with tempfile.TemporaryDirectory(prefix="betta_legacy_weather_") as directory:
    directory = Path(directory)
    inputs = {}
    for name, value in (("legacy", fixture), ("current", current)):
        path = directory / f"{name}.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        inputs[name] = (path, path.read_bytes())
    renders = 0
    for style in ("flat", "glow", "glass", "matte"):
        for step in (0, 1, 2):
            hashes = []
            for name, (path, original) in inputs.items():
                bitmap, dump = directory / f"{name}.bmp", directory / f"{name}.jsonl"
                subprocess.run([sys.argv[1], "--fixture", str(path), "--page", "weather", "--style", style,
                    "--language", "en", "--theme", "dark_v2", "--text-step", str(step),
                    "--screenshot", str(bitmap), "--dump-layout", str(dump), "--exit-after-ms", "300"],
                    check=True, capture_output=True, env=env, timeout=30)
                records = [json.loads(line) for line in dump.read_text(encoding="utf-8").splitlines() if line]
                assert any(r["widget"] == "legacy_forecast" and r["kind"] == "card"
                           and r["type"] == "weather_tile" for r in records), (style, step, name)
                assert path.read_bytes() == original, "Loading must not rewrite the saved fixture"
                hashes.append(hashlib.sha256(bitmap.read_bytes()).hexdigest())
                renders += 1
            assert hashes[0] == hashes[1], (style, step, "legacy forecast differs from current format")
print(f"Legacy forecast: {renders} renders, 12 exact image pairs, saved JSON unchanged")
