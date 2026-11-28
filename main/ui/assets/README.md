<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk | Modified 2026 by andre68723 in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->
# Splash House Image (Optional)

You can replace the boot splash center emblem with a static image.

## Files

Place these files in this folder:

- `splash_house_image.h`
- `splash_house_image.c`

The symbol name must be:

- `splash_house_image`

Example declaration in `splash_house_image.h`:

```c
#pragma once
#include "lvgl.h"
LV_IMAGE_DECLARE(splash_house_image);
```

## Important for performance/stability

- Use LVGL image converter output as C array (not runtime JPG decode on target).
- Keep the image at near-final display size (avoid runtime scaling/rotation).
- Prefer ARGB8888 or RGB565A8 output from converter.

This avoids expensive software image transform paths on ESP32-P4 during boot splash.

# Boot splash logo (BETTA fish)

Boot, Wi-Fi setup, the Wi-Fi coprocessor updater and the panel's OTA progress
screen share `SMART86OS_Betta`. The master is
[`docs/img/brand/BETTA-OS.jpg`](../../../docs/img/brand/BETTA-OS.jpg).
The earlier cyan artwork came from Cpt_Kirk's upstream project and is preserved
at [`docs/img/brand/BETTAOS.jpg`](../../../docs/img/brand/BETTAOS.jpg).
The previous PNG matched all 125,800 pixels of the previous RGB565 array.
Quantizing the cyan JPEG recovered the same codes, allowing either adjacent
code at exact rounding midpoints.

The conversion rounds the 370 × 340 JPEG to RGB565 once. The simulator and
the firmware fallback use its little-endian bytes in `SMART86OS_Betta.c`.
For firmware with `CONFIG_LV_USE_LODEPNG`, CMake selects
`SMART86OS_Betta.png` and its descriptor in `SMART86OS_Betta_png.c`.
The PNG losslessly stores those same pixels expanded to RGB888 by repeating
the high bits. A firmware build links one representation and uses the
checked-in files directly.

The generator rewrites the pixel array inside the checked-in C descriptor
and encodes the PNG from the same RGB565 values. Run these commands from
the repository root to regenerate both assets and verify them against the master.

```sh
uv run --locked --only-group checks python tools/generate_brand_logo.py
uv run --locked --only-group checks python tools/generate_brand_logo.py --check
```
