<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Verify and update the BETTA OS logo

Work on the current `dev` branch of BETTA-HA-PANEL. Read `AGENTS.md` and record the starting commit and working-tree status. Determine where the project uses artwork derived from `docs/img/brand/BETTAOS.jpg`. Replace confirmed uses with artwork from `docs/img/brand/BETTA-OS.jpg`, including the boot splash and OTA upgrade screen if their origin is confirmed. Aim for zero growth in each affected firmware application BIN.

## Trace the artwork

- Follow the boot and OTA renderers in `main/ui/ui_boot_splash.c` and `main/ui/ui_ota_progress.c` to the shared LVGL symbol `SMART86OS_Betta`. Trace its definition through `main/CMakeLists.txt` and `tests/simulator/CMakeLists.txt`. Check which builds use the embedded PNG and which use the raw RGB565 array.
- Read `main/ui/assets/README.md` and `docs/img/brand/README.md`. The documented conversion starts with `SMART86OS_Betta.c` and produces `SMART86OS_Betta.png`. Establish the earlier link to the old JPEG using generator inputs, Git history, or a reproducible decoded-pixel comparison that accounts for RGB565 quantization and byte order. All three image files are 370 × 340, so dimensions alone cannot establish their origin.
- Trace direct and indirect uses across the project, starting with `README.md`, `zensical.toml`, the web editor, simulator and Wi-Fi coprocessor update screen. For each use, record the source, the consuming code or page, and the evidence. Apply the replacement to verified uses. If the firmware emblem has a different or unresolved origin, report that finding and leave that emblem for the owner to decide.

## Apply and verify the replacement

- Use the supplied `BETTA-OS.jpg` as the master. For confirmed firmware uses, derive the PNG and raw fallback from the same target pixels. Document the conversion so another developer can reproduce both. Preserve aspect ratio, colors, positioning, progress text, board-specific scaling and existing fallbacks. Keep historical artwork attribution accurate.
- Reuse the existing decoder and shared image symbol. Try lossless PNG optimization first. Inspect the linker map to confirm each application includes only the intended representation. Keep the partition table and build features unchanged.
- Run the existing boot-splash regression. Render both boot and OTA progress locally with the actual replacement artwork, including the compact 480-pixel and Panel10 layouts. Check that the logo is legible and the progress bar and status text remain visible. Check updated editor or documentation references in a browser.

## Measure and report

For affected firmware, build the baseline and candidate serially with `scripts/build.sh <variant> build`, ESP-IDF 6.1.1, the same effective `sdkconfig`, dependency lock and partition table. Keep all other build inputs equivalent. Report the before and after application BIN sizes, signed byte difference, and free bytes in each OTA slot for each measured variant. List asset byte changes separately. If a faithful replacement still increases a BIN, report the smallest measured increase and the encoding tried. Identify any variants or visual paths that could not be verified.

Finish with the provenance verdict, changed paths, validation results and size measurements. Keep the work local and preserve unrelated workspace changes. Leave staging, commits, pushes and device flashing to the owner.
