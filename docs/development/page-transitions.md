<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->
# Page transitions

`ui/ui_transition.c` uses LVGL 9.6 object animations in the shared page viewport.
A 120 ms ease-out entrance translates content from -24 px to zero (bounded to
one quarter of a smaller viewport), without overshoot. Popups and subpages fade
in over 160 ms and out over 120 ms, retaining a short, visible transition.
`ui.page_animations` defaults to true and is persisted with runtime settings.
Both native System settings and the web Design page edit it; the backup includes it.

Every navigation path reaches `ui_pages_show_index()`. The outgoing page hides
immediately, so its camera worker and widget timers follow the existing visibility
callback. The current page ID changes once, before the transition completes.
A repeated selection does not restart animation or refresh. A new destination
finishes the previous transition; invalid targets leave it alone. Initial display
and layout/theme/orientation rebuilds skip the entrance. A temporary viewport
input shield blocks moving controls while leaving the header and tabs usable.
Deletion and rebuild cancel the animation and remove the shield.

Only the UI task or code holding the display lock calls this module. Animations
use the object as their LVGL lifetime owner, without snapshots or a second
framebuffer. Ordinary opacity avoids `opa_layered`'s blend buffer. A small fade
record tracks completion; a closing input shield and event filter prevent repeated
actions. Owners keep their handles/context until deletion; teardown still deletes
synchronously. Settings stops its brightness preview before closing. Reusable
Xiaozhi dialogs hide after fading and can reverse a close when reopened. Disabling
animations settles active fades. Replacement notifications retire an old dismissal.
Boot, OTA and screen-off/wake retain their existing lifetimes.

The behavior was reviewed in [SmartPanel's UI source](https://github.com/betamoojw/SmartPanel/blob/30bcd774b945123ca69315e3d6cb5db78f80960c/lib/ui/src/ui.c)
at `30bcd774`: LVGL 8.3.11/SquareLine content entrances and screen fades.
This is an independent LVGL 9.6 implementation; generated source, image assets
and the reference's same-screen gesture reload were not imported.

`--regression transitions` exercises the real LVGL page manager through all four
rotations: initial display, repeated/invalid targets, rapid switching, disabled
animations, parent deletion, rebuild and fade completion. CTest and `check.sh`
include it. `--regression popups` checks real popup open/close events in both modes,
duplicate actions, timers, replacement and owner deletion; the Xiaozhi lifecycle
test covers reusable dialogs. Settings and storage round trips are tested separately.
Host and simulator results do not establish physical refresh rate or touch behavior.

On the Waveshare LCD-X 10.1, `drivers/display_present.c` assembles rotated
partial updates in the spare DPI framebuffer and submits only complete frames.
It reuses the BSP's two RGB565 framebuffers; a completed-frame sequence, rather
than a stale semaphore token, gates reuse of the previous front buffer. Two
completion events after submission cover the IDF DMA interrupt/submission race.
Submission returns immediately, allowing the UI lock to be released; the next
write waits only if completion is still outstanding. The one-second deadline
aborts with diagnostic evidence if completion stops. Only the changed bounds
are repaired in the released buffer before the next update, avoiding a full
frame copy for small changes such as clock ticks.
CPU rotation and native touch mapping remain shared with the existing path.
Host checks cover partial preservation, completion races, timeout and failed
submissions. On 2026-10-29, the LCD-X v3.2 unit at `192.168.71.120` ran
`9c8c1b55-dirty` / `20:42:09` through app-only OTA and reported `valid`.
At saved 180° rotation, the owner confirmed that horizontal tearing disappeared
and page slides, settings fades and touch worked normally. Disabling animations
gave immediate transitions; web off/on saves survived reload. Settings, layout,
English, Classic Emoji and recorded local backups were preserved. This bounded
trial did not measure frame rate or establish other boards' physical behavior.

The final popup trial on the same unit used `9c8c1b55-dirty` / `21:57:33`
on 2026-10-29 (app SHA256 `108af8aee6920606...`). The owner accepted the
160/120 ms Wi-Fi information, System settings and Xiaozhi fades as visible and
responsive, without tearing or touch problems. Animations were then restored
off through the web checkbox and verified after reload. All 15 final checks
passed, including settings/layout preservation, English, Classic Emoji, 180°,
local backup hashes, valid OTA, HA connection and no unexpected restart/crash.
The matching source passed 4,767 popup/page/settings SDL checks across wide and
compact geometry plus the five-geometry Xiaozhi lifecycle suite; 351 browser
checks and the other popup regressions were retained from the prior build.
Other popup families have local simulation coverage, not individual physical
acceptance. Windows heap-accounting history/notification suites were excluded;
their real popup events are covered by `--regression popups`. No stress test ran.
