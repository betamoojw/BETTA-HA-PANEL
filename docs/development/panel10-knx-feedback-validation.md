<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->

# Panel10 KNX feedback validation

The 2026-11-26 trial used the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 LCD-X
Rev 3.2 panel on COM24. The device ran the v0.9.2 Panel10 build
from the `e24c81a0` workspace with pending KNX changes, built with ESP-IDF
6.1.1. Its panel build stamp is **Nov 25 2026 23:36:56**. The IDF application
descriptor records 23:22:53. The application is 6,812,512 bytes, leaving
3,232 bytes in the 6,815,744-byte application slot.

| Artifact | SHA-256 |
| --- | --- |
| Application | `dfd7244610e350a5d47a4f6f3386913ea7f9b65b99280aec8f7be7ce7d63f4ca` |
| Matching ELF | `bcfd4cdbce398494896d00cd32d351dcf1f77f490b9bef4f098b2b9992fdcc54` |

## Backup

Before entering download mode, the authenticated editor API reported the
21:17:30 build running from a valid `ota_0`. The backup included the layout,
configuration, all 76 KNX objects, diagnostics and crash inventory. No crash
reports were listed. An unauthenticated layout request returned HTTP 401.

The ESP32-P4 flasher stub checked the installed partition table and read the
metadata (including NVS and OTA selection), coredump partition and all
19,529,728 bytes of LittleFS. The first high-speed serial attempt failed a
block checksum before any write. The retry at 921,600 baud verified the saved
bytes against digests computed on the panel. Mounting the backup on the host
found eight files. Its canonical layout matched all nine pages and 45 widgets
exported by the editor after removing the three view-only geometry fields.
There were no graph widgets or stored `.grph` history files to compare by sample.
The stored KNX registry also matched the authenticated export byte for byte.

The complete old `ota_0` was archived before writing. The only written range was
`0x020000–0x69ffff`. A separate read of all 6,815,744 bytes reproduced the pinned
application followed by the original 3,232-byte tail, with whole-slot SHA-256
`6498474dbbb15da2e501a5637531ed502c99b04037c78e71e878293b8cd918f2`.
Before reboot, the protected metadata, LittleFS, coredump partition and
inactive `ota_1` still matched their pre-write flash digests.

The 100.099-second COM24 log starts at host-triggered reset. The bootloader
loaded offset `0x20000` at 1.317 s and the app printed the matching ELF prefix
`bcfd4cdbc`. HTTP started at 22.832 s, KNX loaded all 76 original objects at
24.529 s and the startup overlay closed at 25.176 s. Authenticated status
reported `e24c81a0-dirty`, `23:36:56`, valid `ota_0`, next slot `ota_1`
and idle OTA. Boot count advanced once from 260 to 261 and stayed there after
the exercise. Safe mode was off.

## Isolated actuator

The test uses a host-simulated actuator and the panel's normal KNXnet/IP
Routing service on `239.255.71.176:43671`. The simulated actuator uses
individual address `15.15.198` and the panel uses `15.15.210` during the
exercise. Both the group and UDP port differ from the household route.

| Test object | Group address | DPT | Panel access |
| --- | --- | --- | --- |
| Level command and its separate writable alias | `30/7/201` | 5.001 scaling | Write |
| Independent level report | `30/7/202` | 5.001 scaling | Read |
| Angle command | `30/7/203` | 5.003 angle | Write |
| Operating preset command | `30/7/205` | 20.102 HVAC mode | Write |

## Device and editor observations

The test appended a four-widget page to the original layout. Chrome loaded the
editor directly from the panel. At each observation the test saved the rendered
editor labels, a web capture, a 1280 × 800 PNG from the panel's LVGL display
and the object cache. The separate host receiver recorded actual routing frames,
including source address, direction, APCI and payload. HTTP 202 only counted
as queue admission.

| Action | Panel and editor result | Routing evidence |
| --- | --- | --- |
| Open the command-only slider before the first write | Both level widgets showed `--`. | Neither writable object had a last command. |
| Use Live Display pointer input on the native slider to select 80%, then 0% | Both widgets showed 80%, then 0% on both displays. Reloading the editor on the same boot retained the empty binding and showed the cached zero. | The simulated actuator received GroupValueWrite payloads 204 and 0. |
| Add feedback to the same slider, then send 80% | The bound slider showed `--` pending feedback. The command-only alias showed 80%. | The panel's own write did not create feedback on the independent address. |
| Simulated actuator reports raw 158 | The bound slider showed 62% on both displays. Writing 20% through the alias later changed only the command-only display to 20%. | The separate source sent GroupValueResponse on `30/7/202`, decoding to 61.96%. |
| Select 270° and Economy | Two consecutive pairs of captures showed 62%, 80%, 270° and Economy with no new telegrams between captures. | Angle and preset command payloads were 191 and 3. |
| Disable and re-enable isolated routing | Sliders cleared. A write while disabled returned HTTP 400. The rejoined route stayed unknown until fresh data arrived. | After a new 80% write and raw-158 response, the displays again showed 62% and 80%. Fresh zero commands displayed 0° and Auto. |

On the **23:36:56 device build**, disabling routing made the native preset tile
show `unavailable` and the editor show `--` (capture 09). Both showed `--` after
rejoining, before any new telegram (capture 10). A fresh command with value 0
made both show Auto (capture 11).

The browser, panel PNG and registry were sampled successively. In capture 06
the editor showed 80% for the shared command. By the registry read it was
32%, while the independent feedback still decoded to 62%. Captures 14 and 15
verified 80% and 62% together on both displays with a stable registry.

The first angle snapshot crossed an additional panel-originated write and
showed 83° while the preceding web reading was 270°. That pair was excluded
from the 270° result. The follow-up captured both surfaces twice while the
bus was quiet. The first slider pointer calibration selected 79% and 81%.
The final native input reproduced exactly 80% and 0%, confirmed by the receiver.

## Restoration and scope

Both test passes restored the simulated level command, level report, angle and
preset to their initial wire values of 64, 64, 128 and 1. The original route
was running again with all 76 objects. The final authenticated layout and KNX
exports matched the pre-flash files byte for byte. The account list, themes,
language inventory and camera configuration also matched. The administrator
could reopen all nine pages, and anonymous layout access still returned HTTP
401. English, Asia/Shanghai, 12-hour time, Glow, the 180-second flip clock
and the 23:00 night start were retained.

The original time server was retained. The 8-second SNTP wait timed out on
startup. The device still reported an unsynchronized clock at uptime 414 s.
At uptime 541 s it reported valid time without a setting change or a second boot.

LVGL snapshots and remote pointer events exercised the panel slider. The simulated
actuator supplied differing feedback. The isolated route stopped and rejoined
with Wi-Fi connected. Physical actuators, router power loss and physical touch
were outside this trial.

All 12 host CTest cases passed with SDL on the test process's path. Two browser
drivers timed out after device checks and restoration. Closing the input pipe
fixed teardown. A read-only pass verified all 15 capture pairs and the restored layout.

## Local regression after the COM24 trial

The generic entity formatter rendered both offline and unknown presets as `--`.
The previous source failed the English and Chinese offline assertions. Using the
KNX unavailable translation in Select made both pass.

| Local test | Result |
| --- | --- |
| Editor preset, DPT 20.102 | Offline showed **Unavailable** / **不可用** and hid the picker arrow. Rejoining showed `--` and enabled explicit selection. Fresh 0 showed **Auto** / **自动**. With independent feedback, a command alone left the preset unknown until live feedback arrived. |
| KNX worker, editor and Panel10 SDL | The worker and editor agreed with 16 native slider labels across eight snapshots, including 80%, 0%, independent 62% feedback, shared commands, failed sends and registry replacement. The angle slider exercised 270° and 0°. |
| Home Assistant Select | The editor kept literal option names for both `select` and `input_select`. In SDL, each sent `select_option` to its own domain and retained the previous option until HA reported the new one. |

All 687 web tests passed with the worker and simulator enabled and none skipped.
All 12 host CTest cases and the C layout save/reload test passed. The Panel10
LVGL/SDL controls run reported 260 KNX checks and 387 general control checks.

Windows and ESP-IDF 6.1.1 produced the **Nov 26 2026 20:15:12** application.
Its checksum, embedded ELF digest and compressed editor matched the build artifacts.
Chrome replayed eight archived registry snapshots using the packaged editor
(`js/app.4fed3f2ec0ce940b.js`). All 80 English and Chinese labels matched,
including the 32% shared command present at the capture 06 registry read.

| Bytes | Nov 25, 23:36:56 (COM24) | Nov 26, 20:15:12 (local) | Change |
| --- | ---: | ---: | ---: |
| Application | 6,812,512 | 6,812,544 | +32 |
| Free in either 6,815,744-byte OTA slot | 3,232 | 3,200 | −32 |
| Compressed editor JavaScript | 568,268 | 568,291 | +23 |

| Local artifact | SHA-256 |
| --- | --- |
| Application | `d6e61f06b9bb9fbef7a4d84b5965977d5f62e22c3a7c64b9d01606469951517c` |
| Matching ELF | `282ad650301398ed01125bbff45235ce0e36d61a22f9540cd6231f0428d41b45` |

## COM24 retest of the 20:15:12 build

The 2026-11-26 retest used the same Waveshare LCD-X panel. Before entering
download mode it ran the 23:36:56 build from a valid `ota_0`. The authenticated
backup covered nine pages, 76 KNX objects, settings, users, diagnostics and the
crash inventory. Anonymous layout access returned HTTP 401.

Two snapshots captured different layouts. The web export contained 47 widgets.
The later 19,529,728-byte LittleFS backup contained 46. The only difference was
the `page_5_graph_1` widget (**Light Angle**) on Page 9. Both snapshots were
retained, and the owner selected the 47-widget web export. The mounted backup
contained eight files, including the original KNX registry, and no `.grph`
history files or pending graph writes to compare by sample.

The flasher verified the old `ota_0` backup against the panel, then wrote only
`0x020000–0x69ffff`. A fresh serial read of all 6,815,744 bytes matched the
6,812,544-byte 20:15:12 image followed by the original 3,200-byte tail. The
whole-slot SHA-256 was `dcc921719067ce9c05572dd1f3ff615a5c4f7ad090c2ad68fb688394cca856f5`.
The inactive `ota_1`, metadata/NVS, crash storage and LittleFS still matched
their pre-write digests before the application was started.

The 100.177-second COM24 log shows `0x20000` loaded at 1.317 s after reset,
ELF prefix `282ad6503` at 2.790 s, HTTP listening at 16.332 s, 76 KNX objects
loaded at 18.073 s and the startup overlay closing at 18.710 s. The authenticated
API reported v0.9.2, `e24c81a0-dirty`, Nov 26 2026 20:15:12, valid `ota_0`,
next `ota_1` and idle OTA. Boot count advanced from 261 to 262. The editor
JavaScript served by the panel matched the asset embedded in the pinned image.

After boot the panel returned the 46-widget disk layout. Restoring the owner's
47-widget export matched the original HTTP backup byte for byte. The test
then appended one page to that layout and kept all original widget definitions.
The host actuator and the panel used `239.255.71.177:43672` for the trial. They
reused the isolated group addresses from the first trial and added `30/7/206`
for independent preset feedback (readable DPT 20.102).

### Operating preset on the panel and in the editor

Chrome loaded the editor from the running panel. Both preset tiles used the
same command. Only the second had a feedback binding. Before disconnecting,
the first showed Economy and the second showed Comfort. Disabling and
re-enabling the isolated routing service produced the following sequence in
both English and Simplified Chinese.

| Action | Command-only preset | Preset with independent feedback |
| --- | --- | --- |
| Disable routing | English panel `unavailable`, editor **Unavailable**. Both showed **不可用** in Chinese. | Same offline label. Both editor picker arrows disappeared. |
| Re-enable routing before a new telegram | `--` | `--`. Both editor picker arrows returned. |
| Send a new command with value 0 | **Auto** / **自动** | `--` |
| Receive a new GroupValueResponse with value 0 on `30/7/206` | **Auto** / **自动** | **Auto** / **自动** |

The owner saw **不可用**, then `--`, then **自动** on the physical panel.
The panel PNGs, editor captures and object cache also distinguish the
intermediate step in which only the command-only preset displayed Auto.

### Sliders and restoration

| Test | Result on the panel and in the editor |
| --- | --- |
| DPT 5.001 with empty feedback and a second widget on the same command address | Both showed 80%, then 0%. The actuator received payloads 204 and 0. The empty role and displayed zero survived an editor reload. |
| Add independent feedback, then send 80% | The bound slider showed `--` until the actuator returned raw 158 on `30/7/202`, then showed 62%. The shared command-only slider showed 80%. A later 20% command through the alias left the bound slider at 62%. |
| DPT 5.003 angle | Both showed 270°, then 0°. The actuator received payloads 191 and 0. |

The retest used the panel's command API. The isolated receiver recorded 20
routing telegrams, including the panel's writes and the actuator's replies.
All 16 observations have paired editor and 1280 × 800 panel PNGs with unchanged
object state and no intervening telegrams. Across 11 English observations,
all 44 checked native label regions matched the earlier panel reference pixels.
The actuator ran on the host. A physical KNX actuator and a physical router
outage remain untested.

After the test, the simulated actuator returned to its starting values and
the original route was running with all 76 objects. The final nine-page,
47-widget layout matched the selected web backup byte for byte, including
**Light Angle** and its `light_angle` feedback binding. The KNX registry, users,
themes and camera configuration also matched their backups. The settings
comparison found only the current time and Wi-Fi signal had changed. English,
Asia/Shanghai, 12-hour time, Glow, the 180-second flip clock and the
23:00–06:00 night schedule were restored. At uptime 285 s, boot count was
still 262 and `ota_0` was valid. After the test session signed out, anonymous
layout access returned HTTP 401.

## Pre-commit review on the host

The 20:15:12 archive passed a second hash and content check covering the slot,
16 paired captures, 20 telegrams and the restored 47-widget layout. New local
tests reproduced mixed routing sessions across registry pages, inconsistent
shared-address readings and an older picker reply replacing a fresh Auto.
After the fixes, reads spanning a reconnect restarted once, aliases used the
latest observation for each direction and the newest completed response prevailed.
Reserved HVAC reports cleared bound readings. A fresh zero restored Auto.
Command-only switches and sliders started at `--`, and a first card tap on a
command-only switch queued a toggle. Failed or pending sends retained the previous value.

All 691 web tests, 12 CTest cases and the C layout persistence test passed.
Panel10 SDL passed 273 KNX and 388 general control checks. Chrome replayed all
16 archived COM24 observations with the rebuilt editor, matching 80 labels and
every preset arrow. The ESP-IDF 6.1.1 **21:12:10** image is 6,813,280 bytes,
leaving 2,464 bytes in each OTA slot (736 fewer than the device-tested build).
Application SHA-256 is `b9c5b32b729cd607e00057b8c90eacc84ad70576a8cecd14be95bc92551c93c9`.
This image and the added reconnect, alias and invalid-report cases have local
coverage only. The physical panel verification above used **20:15:12**.
