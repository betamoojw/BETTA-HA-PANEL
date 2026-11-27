<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -- file added in the andre68723/BETTA-HA-PANEL fork, see docs/release-notes.md -->

# Panel10 CJK font validation

## Windows build comparison

Both applications were built on Windows with ESP-IDF v6.1.1 at the same
source timestamp. The effective `sdkconfig`, Panel10 dependency lock and
generated partition table matched byte for byte. Both OTA slots held
6,815,744 bytes.

| Windows build | Application BIN (bytes) | `ota_0` free (bytes) | `ota_1` free (bytes) |
| --- | ---: | ---: | ---: |
| `c86463d7` baseline | 6,814,992 | 752 | 752 |
| Zopfli patch on `c86463d7` | 6,811,520 | 4,224 | 4,224 |

The complete application shrank by **3,472 bytes**. A control build with
the original stream and the same `-dirty` source label as the patch also
measured 6,814,992 bytes. The embedded stream itself shrank by 3,475 bytes,
from 192,227 to 188,752. Both streams expanded to the same TTF. The runtime
decoder source and linked zlib library matched across builds.

On Windows, `tests/tools/test_pack_cjk_font.py` passed all five tests.
These included 136 checks of the production C decoder covering corrupt input,
allocation failure, recovery and cache reuse. The raw font accessor used
by the other boards also passed.

## Windows image and COM24 readback

On November 27, 2026, the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 LCD-X
Rev3.2 on COM24 received the previously verified Windows Zopfli application.
Its source label was `c86463d7-dirty` (the six-file patch on `c86463d7`).
ESP-IDF v6.1.1 built the v0.9.2 image with the fixed application stamp
`Nov 27 2026 14:42:58`. The previous application on the panel was
`5ca8249d`, stamped `22:13:34`. The fixed compilation time does not give
the order of installation.

| File or device read | Bytes | SHA-256 |
| --- | ---: | --- |
| Windows application BIN and the application bytes read from COM24 | 6,811,520 | `591b5bb285625890cdb848f265a1e70e1db77db0f3abd6bb44f711bdfd3cb73e` |
| ELF identified by the application descriptor | 48,842,372 | `0130c9fbe13501567773e08823ac0ae4006b88a551f52947c124511a651f31e3` |
| Original active slot backup | 6,815,744 | `770824da91076e24277847171fcf080fadfb41b86b0284ec9fa6e2b517b2b12b` |
| Independent read of the complete new `ota_0` slot | 6,815,744 | `a1faf7e37c4f0fe1a6fad4a8e7028e0feea294f836b6553d25c48d7105d037ab` |

The authenticated OTA status and the CRC-checked OTA selection record
(sequence 23, valid) both identified `ota_0` at `0x20000`. The device
partition table matched the archived build. The serial write covered
`0x020000–0x69ffff` only. All bytes read back from that slot matched the
Windows BIN followed by the 4,224 original trailing bytes.

The 188,752-byte stream at offset `0x229a48` in the **serial slot readback**
matched the staged Zopfli file byte for byte. It decompressed to the original
286,900-byte TTF with SHA-256
`545ba73beea4cd33d5e9577555a45f6f3d81be3de4176f3402b4dc58797acb9e`.

The device-side digests of the inactive `ota_1`, boot metadata and NVS,
crash storage and LittleFS matched their pre-write values before the
application was released from reset. At 1.317 seconds from the reset
request, UART reported loading the application at `0x20000`. At 2.790
seconds it printed ELF prefix `0130c9fbe`, followed by ESP-IDF v6.1.1.
The UI logged showing and hiding the boot splash at 8.974 and 20.970
seconds. Those elapsed times measure the software handoff.
The authenticated version reported `c86463d7-dirty`, `valid` and safe
mode off. OTA status reported `ota_0` running, `ota_1` next and `idle`.
The served editor JavaScript matched the copy embedded in the flashed BIN.

## Backups and graph coverage

Before opening COM24, the authenticated editor exported a restorable backup.
Independent device reads agreed with its layout, settings, camera configuration
and KNX registry. The acquisition also captured the account list, themes,
language catalogs, task inventory, device log, reboot history and crash index.
The crash index contained zero reports.

The pre-write serial backup covered the original active application, boot
metadata including NVS, crash storage and all 19,529,728 bytes of LittleFS.
Unchanged blocks from the previous local archive were reused only after
checking device digests. The reconstructed regions were checked against
whole-region device digests. The mounted filesystem contained eight files.
Its layout and KNX configuration matched the authenticated reads. The
**Light Angle** and **Power history** chart bindings were present. There
were no local graph-history sample files for a sample-by-sample comparison.

## Simplified Chinese on the panel

The operator kept the original dashboard. An authenticated settings update
temporarily selected `zh-cn` from the original `en` language. Navigation
requests opened two existing pages without activating their widgets.
The 1280 × 800 PNGs came from the running panel's LVGL screenshot endpoint.

| View | Legible examples in the device PNG | PNG SHA-256 |
| --- | --- | --- |
| Environment | Two 厨房 labels, 当前, 目标 and 正在供暖 | `fe92785d0b93a0c1ac0b8177d79fbd604548b4a05977a9a5e6fb39006d23d2cd` |
| Weather and navigation | 晴夜, 今天, 周六 and the 客厅, 天气 and 信息 tabs | `43400d49d900c9199dc3e8d86809f5f0aa68e845dd4297cb78e8999ee3610c43` |

Independently of the PNGs, the operator confirmed that the characters
were clear and complete on the physical LCD and that touching the tabs
switched from Weather to Living Room and back. The UART capture also
logged rendering `room_1` and `page_1` during the touch check.

## Flip clock and final state

The test crossed midnight in the panel's `Asia/Shanghai` time zone.
During the configured 23:00–06:00 night interval, the display protection
logic selects screen-off on inactivity. With the owner's approval, the
test temporarily disabled the night switch at 00:01:46 on November 28.
The clock kept its 180-second delay, 12-hour format, date, seconds and
flip style. The panel used its original Glow theme and had already
accumulated sufficient idle time when the schedule was paused.

Two full-screen device PNGs showed **星期六, 2026/11/28**, the correct
Saturday, at **12:01:47 AM** and **12:01:51 AM**. The date and AM label
remained fixed. The 7,325 changed pixels were confined to the two seconds
cards. The operator independently confirmed clear date and weekday glyphs,
a centered clock, continuous flipping and touch wake without activating
an underlying control. The final font counters showed 37 instances,
zero budget hits and zero creation failures.

| Clock screenshot | SHA-256 |
| --- | --- |
| 12:01:47 AM | `1b28e375c6597548aeb8d26a5e4a3f1d6af7f1579d6a65a1901e2a0118c4aff9` |
| 12:01:51 AM | `d0add6f713bb6069f8d18b31f53445546064888674cf492fe3ad4c9f6201271e` |

At 00:02:41 the settings readback confirmed English and the original
enabled night schedule. The final editor backup matched the original in
every field except its export timestamp. Independent API reads also
confirmed the original 10 pages, 61 widgets, both chart bindings, 76 KNX
objects, accounts, cameras, and language and theme catalogs. Only the live
clock and Wi-Fi signal varied in the settings response. After the test
session signed out, the protected layout endpoint returned HTTP 401.

COM24 recorded **549.809 seconds** from the deliberate restart, including
1,145 complete UART lines. The reboot log advanced once, from 270 to 271.
The final check still showed the same application and valid `ota_0`,
with no rollback or new crash report. The glyph check covered the displayed
Simplified Chinese examples. Other languages and the rest of the font's
character repertoire were outside this physical test.
