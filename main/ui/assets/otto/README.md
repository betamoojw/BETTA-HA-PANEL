<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Otto robot animations

Derived from `txp666/otto-emoji-gif-component` 1.4.0 (MIT, copyright 2025 小鹏),
revision `4279001383c2b1a39ed0457b14759e7c87537941`:
[component](https://components.espressif.com/components/txp666/otto-emoji-gif-component/versions/1.4.0/readme).
The original license is retained in `LICENSE`.

The 21 animations are resized from 240 to 96 pixels, with the original 16-level
grayscale, expression names, loop timing and black face background retained.
This keeps the app inside the existing OTA partition. The popup's surrounding
surface still follows the system theme. `manifest.json` records source/output
hashes and each loop duration. Source archive SHA256:
`3e25c9638d3e0a949ecc545813bda220599835d8d3220db1c290780a3ab86bfd`.

Regenerate with Pillow 12.3.0:

```sh
python tools/generate_otto_assets.py --source /path/to/otto-emoji-gif-component
```

Firmware and simulator builds generate C arrays from these maintained GIFs with
`--embed`, using Python's standard library only. Only `panel10` firmware embeds
them. The guarded HTTP endpoint and LVGL renderer use those same bytes.
