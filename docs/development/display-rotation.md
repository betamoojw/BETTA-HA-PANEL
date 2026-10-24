<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->
# Display rotation

## Settings and geometry

`hardware.rotation` stores 0, 90, 180 or 270 degrees relative to the board's
default. Missing values preserve the existing pose. HTTP rejects other values;
native settings merge only changed fields. `hardware.auto_rotation` is available
only on Tab5 and retains the manual angle while enabled.

`display_orientation` applies an absolute LVGL rotation after settings load at
boot. At runtime the UI task retries the saved manual target once per second,
outside flash writes and under the display lock. Visible top-layer popups,
native settings and an active screensaver defer the change. Hidden screensaver
objects do not block it. Failed layout rebuilds restore the previous geometry.
Tab5 keeps its bounded IMU sampling and stable-orientation memory.

Default native rotations are 270 for panel10, 90 for JC/Tab5 and 0 for square
panels. `display_geometry` exposes the current logical dimensions. Layout
projection uses the board's short edge, 800 or 720, for portrait width and
preserves independent portrait rectangles. Pages can scroll without shrinking
text or discarding entities. The editor loads projected dimensions and retains
unsaved work until an explicit reload; a stale-orientation save returns HTTP 409.

## Rendering and touch

P4 boards use the shared DSI bitmap callback at every orientation. Panel10 and
original JC retain CPU rotation to avoid the previously observed PPA timeout.
Panel4 reserves rotation scratch in PSRAM even when starting at zero degrees.
S3 retains direct DMA scanout and rotates partial packed RGB565 areas into one
additional 9,600-byte internal scratch buffer before the framebuffer copy.
No scanout timing, byte order or physical touch calibration is changed.

Physical touch drivers continue to report calibrated native coordinates; LVGL
maps them through the same absolute rotation. Live Display invalidates its
session when the display resolution event fires, releasing old remote input.
Its next connection uses the new geometry. Screen protection uses a relative
overlay size so it follows both landscape and portrait.

The optional Xiaozhi action shares the right-hand topbar layout with connection
and settings icons. Its actual width is reserved before positioning the clock
and left-hand slots, including AP, MQTT and crash-warning combinations. A fixed
offset from screen center overlapped HA in the enabled 800-pixel portrait view.

## Validation

Local checks on 2026-10-24, based on `e88cdfe` with uncommitted rotation changes:

- Panel10 ESP-IDF 6.1 build passed, with CPU rotation retained.
- 319 browser tests passed, including quarter-turn save events, automatic-mode
  disable/re-enable and reset. Static WebUI checks passed.
- 61 targeted CTest cases passed: five board geometry profiles, CPU pixel
  rotation, native settings, clock protection and remote capture/input.
- The existing host orientation suite passed boot/fallback, sensor dwell and
  hysteresis, retry policy, geometry and layout round trips.
- Native portrait settings passed 1,151 assertions at 800 x 1280, including
  four manual angles and save/reopen, with seven languages.
- After the topbar fix, the portrait run passed 1,207 assertions, including
  the enabled auxiliary action across connection/status combinations. Both
  native-settings and Xiaozhi lifecycle CTest cases passed; Panel10 rebuilt.
- Widget bounds checks passed: 1,866 renders on panel4, 1,818 on JC landscape
  and 1,818 on JC portrait. Energy and House portrait captures were inspected.
- Local browser controls were checked at 1440 x 900 and 390 x 844. No horizontal
  overflow or controls escaping their groups; the manual angle survived mode
  changes and reload. Browser data was synthetic and saved only locally.

## Panel10 device trial

On 2026-10-24, the COM24 Waveshare ESP32-P4 v3.2 unit received the rotation build
and a follow-up fix for the enabled Xiaozhi topbar action. The final image is
based on `e88cdfe8-dirty`, with ELF SHA256
`cd27a47b5219495377326f0ec30a79d99793477e123a94dc08133e9f6d689194`.
The application SHA256 is
`a4ff1ef8f04ef4c1360c2cf4091602fff833d6e16a71778b73c418805b27d84e`.
Both app-only writes used the verified P4 stub: metadata/NVS, crash storage and
all LittleFS bytes matched before/after writing, before boot. Startup ELF, source
stamp and valid OTA state were checked; HA reconnected with existing credentials.

All four relative angles were selected through the real WebUI and through native
System Settings using remote pointer input. Saves, reopen and projected layout
dimensions passed. Live Display returned frames at 1280 x 800 for 0/180 and
800 x 1280 for 90/270. A controlled restart retained 270 degrees, then native
settings restored 0. Rotation deferred during the clock screensaver and applied
after wake. The original settings, English, layout and themes were retained.

The initial continuous run covered 33 status samples over 504 seconds, with no
unexpected restart or new crash report; monitoring continued after the deliberate
persistence restart for 21 samples over 304 seconds, also without an unexpected
restart or new report. Rapid scripted remote drags encountered display-lock waits
and closed connections; slower single-motion drags completed the native tests.
This does not establish sustained high-rate remote dragging. The owner confirmed
normal physical output at all four angles, aligned local touch, following Live
Display and no flicker/jitter. Final readback found 180 degrees and clock brightness
56% despite the intended restoration; only those two fields were restored to the
requested 0 degrees and original 68%. A subsequent full comparison confirmed the
original settings, English, layout and themes. The cause of those two changes was
not established. Other boards have not been flashed for this change.
