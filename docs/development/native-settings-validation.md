<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->
# Native settings validation

See [Alert center](alert-center.md) for the shared header alert regression.

### Native screen settings

Run `scripts/simulator.sh <variant> --regression screen-settings --fixture tests/simulator/screen_settings.json`.
It exercises actual LVGL control events, Save/reopen, disabled-option retention, failed-save retry,
unrelated-settings preservation and 100 view rebuilds. Header checks include AP, MQTT and crash-marker
combinations. Every variant accepts native `--rotation 0|90|180|270`; see [Display rotation](display-rotation.md).
The dedicated empty fixture avoids unrelated widget orientation projection during these checks.
CTest covers both real time and the shared fixed simulator clock. Save-result polling, view teardown
and fade completion advance through LVGL's selected delay callback, so both clock modes exercise the same events.
`betta-clock-test <width> <height>` exercises the production saver with a controlled clock: idle/off
deadlines, movement bounds, night priority, complete wake gestures, storage-busy retention and
the 30-second preview deadline. Both tests are registered with CTest.
Clock checks use the real Poppins font at six geometries, standard/largest text and
all date/seconds/12-hour combinations, including movement bounds and cache reuse.

On 2026-10-26 COM24 ran the large digital clock build `45ef89dd-dirty` (ELF `e0680192`).
App-only stub write/readback and protected-storage checks passed; source and valid OTA matched.
The owner confirmed clear time/date, advancing seconds, touch wake, brightness and page switching.
Screenshots verified large digits, a separate date and bounded movement; timed screen-off passed.
One settings restart followed the owner's NTP change, which was retained; no crash was stored.
English, Classic Emoji, 180-degree rotation, layout, themes and local backups were preserved.
Remote wake/brightness also passed. Earlier white flashes remain unresolved despite this bounded pass.

Local validation on 2026-10-24 passed the settings regression at default/largest text on 480 × 480,
720 × 720 and 1280 × 800, plus all four Tab5 directions at largest text (720 × 1280 / 1280 × 720).
Each run covers all seven languages; clock checks passed at four geometries. English and Chinese
wide/compact renders and German portrait/landscape renders were inspected. All 317 browser tests,
built-in translation parsing, font/catalog checks and the strict documentation build passed.
The ESP-IDF 6.1 `panel10` build succeeded (5,652,416-byte app; 17% app-partition headroom).
The tested image was built before commit from `cab3c41` plus the screen-settings changes;
its embedded source stamp is `cab3c412-dirty` (2026-10-24 10:26:01).

Device validation on 2026-10-24 used the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1
on COM24, with app SHA256 `c34548d26fa83a0ddedbb5dcba20a35892276929a74948b0543269c72e3c90cc`.
The supported P4 stub verified the application readback and unchanged protected metadata,
NVS, coredump and complete LittleFS checksums before boot. The running ELF stamp matched;
OTA was valid and the boot splash exited. Native 12-hour Save/reopen and restoration to
24-hour format passed, preserving the latest settings, layout and English language.
The owner confirmed the rightmost settings gear, matching Wi-Fi/HA frames, saving,
stable physical display and aligned touch. Two one-minute runtime samples showed no reboot
or new crash. Rapid remote input encountered transient display-lock contention; paced
input completed the control tests. Local backups and retained crash reports were preserved.

The System settings extension on top of `4a308a6` adds language, time zone, appearance
and built-in/imported theme selection. On 2026-10-24, native events passed at default
and largest text on 480 × 480 and 1280 × 800, plus Tab5 and all four rotations.
Runs cover seven languages (1,063 checks on the compact panel, 1,070 on wide/rotated
layouts), Save/reopen, Cancel, failed-save retry, concurrent unrelated settings,
imported theme names and expanded dropdown bounds. CJK/font, web-source and strict
documentation checks passed; English and Chinese renders were inspected.
The ESP-IDF 6.1 `panel10` build passed (5,662,944-byte app). On 2026-10-24, COM24
ran app SHA256 `1f54dedba317d52abbb952d463433f79966890e3d773155ff0236cb98b5d7890`
with matching ELF and valid OTA state. Stub checks confirmed unchanged protected storage
before boot. Native simplified/traditional Chinese, time-zone, Flat appearance and Light
theme selections passed Save/reopen, with each tested value subsequently restored.
The owner confirmed normal settings/saving, stable display, aligned touch and no unexpected
restart. Seven final samples over 61 seconds showed no reboot or new crash. Restoration
API requests initially used the default settings restart; the local helper now explicitly
disables that restart. Two pre-existing PPA timeout reports from `4a308a6` were retained;
this short acceptance test does not resolve that earlier intermittent display-driver issue.
User edits during testing were backed up separately; English and local backups were retained.
At the owner's request, the final configuration was restored to English,
`America/Argentina/Buenos_Aires` and `dark_v2`, then read back successfully;
all other settings were unchanged. The tested image was built before this commit
with source stamp `4a308a63-dirty`; the binary hash above identifies the device-tested artifact.

### Remote display

The `betta-remote-test` simulator target exercises the production remote-display
module with real LVGL. Run it with width, height, render mode (0 partial, 1
direct, 2 full) and rotation (0–3), for example `betta-remote-test 480 480 0 0`.
It checks complete capture, bounded dirty-region redraw, idle updates, rotated input, read-only default,
physical-touch priority, cancellation, heartbeat expiry, OTA suspension and
reopening. The web tests also cover protocol corruption, edge tiles on larger
screens, reconnects and browser lifecycle cancellation.
Build the target, then run `ctest --test-dir build/simulator/panels3 --output-on-failure`
for its 48 registered geometry/mode/rotation cases.

Firmware and the SDL2 simulator share the LVGL widget sources; the editor has its own checks. Run the local checks
first, then review on hardware.

### Empty-dashboard flip clock

The simulator's `--regression transitions` includes the default flip clock:
six content geometries, four themes and four appearances, Standard/Largest text,
English/Chinese, 12/24-hour changes, midnight/noon, date rollover, unsynchronised
time, initial date font sizing, folding frames and deletion during an animation.
It also checks replacement of the clock by a real page and timer/animation cleanup.
On 2026-10-31, the Windows SDL run passed 22,010 clock checks and 604 navigation checks.
All 466 editor tests and 14 local-editor Python tests passed. Native C checks passed
522 backup/layout assertions, graph-history storage/merge cases, and 215 injected
runtime-storage faults. The graph-store test retains history bytes and a queued
snapshot while every saved page is hidden. Empty-layout deletion intentionally
remains different from hiding pages. These host results do not establish device
graph-history retention without an actual graph on that device.

The 2026-10-31 COM24 check used the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1,
revision 3.2, ESP-IDF 6.1, with the 6,455,056-byte application SHA256
`9be10a3fba54b5f15e2ecc2383b879f3f917e975dd7b285da39f31becd9a18f8`.
The active application was backed up; readback verified the new image, with
metadata/NVS, retained-crash partition and complete LittleFS unchanged before boot.
The running source stamp was `b1f1fcff-dirty`, Oct 31 2026 09:06:22; the image hash
identifies this build because the incremental build retained the earlier source stamp.
OTA stayed valid, HA connected, and no crash or unexpected reboot occurred during
the bounded checks. All-hidden and empty layouts showed the clock, seconds advanced,
12/24-hour and theme/appearance changes applied, and restoring a visible page restored
its content and navigation. The web checkbox Save/reload round trip also passed.
The owner confirmed a centred display without visible artifacts on the first image;
the final image additionally fixes the initial date font sizing, checked in LVGL captures.
All eight saved pages and 35 widgets were restored exactly to their original hidden
state, retaining the owner's 12-hour selection. Other settings differed only in live
time/RSSI. This panel had no graph widgets; graph retention evidence remains host-only.
Reported minimum free stacks after exercising the final image were 7,556 bytes for
HTTP, 13,712 for LVGL and 10,084 for the UI task. Captures are LVGL snapshots, not
physical-framebuffer or touch measurements; this was not a stress test.

Pre-commit review on 2026-10-31 aligned the editor's failed-first-load fallback
with the firmware's empty layout and made clock allocation failure fail the layout
reload instead of marking it applied. Timer creation failure also deletes the clock.
The expanded regression checks both folding halves, settling an unchanged target,
and all captions even when the test geometry exceeds the host display. Each of the
panel4, Tab5 and paneljc simulator builds passed 25,082 clock and 604 navigation checks.
All 467 editor tests passed; the four native C suites above were rebuilt with strict
warnings and passed again. The fixed-clock Glow render remained byte-identical.
Web/source/field checks and the strict documentation build passed. The panel10-only
firmware build passed (6,455,136-byte app). This review did not access or flash the
device: the image identified above remains installed with the owner's saved settings.
