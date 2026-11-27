<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->

# Panel10 KNX backup restore validation

The November 27, 2026 test used the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1
LCD-X on COM24 and the editor served by the panel. All times below use UTC+8.
The [registry capacity record](panel10-knx-registry-validation.md#com24-restore-limit-in-english-and-simplified-chinese-november-27-2026)
covers the earlier **21:25:04** build.

## Build and backup

ESP-IDF v6.1.1 built `panel10` from clean commit `5ca8249d`. An initial
incremental build retained an older timestamp. Recompiling both timestamp
objects produced the application descriptor at **22:13:33** and the panel's
build stamp at **22:13:34**. The application occupied **6,814,400 of
6,815,744 bytes**, leaving **1,344 bytes** in the slot.

| Artifact | SHA-256 |
| --- | --- |
| Application | `2110494257dc2d3c9f68f624111fcca9368a9f57ef3d52d6cea7b27f9392603c` |
| Matching ELF | `5cad570e32d5f154286e4d6abd425bc4bf940345af7e391a9da0112825dfd0cb` |
| Independent COM24 readback of the entire `ota_0` slot | `770824da91076e24277847171fcf080fadfb41b86b0284ec9fa6e2b517b2b12b` |

Before entering download mode, the authenticated editor exported a 62,731-byte
backup. Its 10 pages, 61 widgets, 76 KNX objects, camera configuration and
settings agreed with separate API reads. The anonymous layout request returned
HTTP 401. The panel reported boot 269, a valid `ota_0` image, idle OTA and an
empty crash inventory.

COM24 identified the ESP32-P4 v3.2. The OTA selection record had a valid CRC
and selected `ota_0` at `0x20000` (sequence 23). The backup covered the running
slot, boot metadata and NVS, crash storage and the 19,529,728-byte LittleFS.
Device checksums validated the archived blocks reused for the backup. The
mounted filesystem held eight files. Its layout and KNX configuration agreed
with the authenticated exports. It contained no `.grph` or `.grph.tmp` files
for a sample-by-sample history comparison.

Only the active slot was written. All 6,815,744 bytes were then read from COM24
and matched the new application followed by the original 1,344 tail bytes.
The inactive slot, metadata, crash storage and LittleFS had the same device
checksums before the write and before the restart. The bootloader loaded
`0x20000`. Serial reported descriptor time **22:13:33** and ELF prefix
`5cad570e3`. At 39 seconds of uptime, `/api/version` reported v0.9.2,
commit `5ca8249d`, panel build time **22:13:34** and image state `valid`.
`/api/reboot-log` recorded boot 270. `/api/ota/status` reported `idle` on
`ota_0`, with `ota_1` as the next update slot.

## Restore attempts and object 512

The panel and a simulated actuator used a separate KNX routing group and UDP
port from the household installation. The API returned the 512 distinct test
objects in 26 pages. Object 512 was `qa_cap_511` at `30/7/255`, a readable and
writable DPT 5.001 percentage. Chrome 158 downloaded the panel's backup. The
sole content change was the appended 513th object. Both restore clicks used the
same 126,405-byte file, SHA-256
`d7fc9a32aef34574a0111a3ea19c2dd6ec9cc30f54e508c591debd768c537cab`.

| Language at restore | Visible message after clicking **Restore backup** | Subsequent KNX write | Independent reply to the next KNX read |
| --- | --- | --- | --- |
| English | A KNX registry supports at most 512 communication objects. | 80% (raw 204) | 61.96% (raw 158) |
| Simplified Chinese | KNX 注册表最多支持 512 个通信对象。 | 0% (raw 0) | 25.10% (raw 64) |

From each click through its post-refusal readback, the browser recorded **zero
confirmation dialogs and zero write requests**. Both request interception and
request events were monitored. In both languages, the before and after reads
returned the same bytes for each resource. The registry stayed at generation 2
and routing epoch 3.

| Resource read before and after each restore | SHA-256 across all four reads |
| --- | --- |
| 10-page layout | `b14c3c3dc9adb56498241a024326ff785485a2196237b0f7d967de137043f196` |
| 512-object registry | `816d6b717d4e3166438c9fd3d0e5bb16eca92185618098d5a76e7dc44e0f6267` |

The selector's autosave intervals below run from forwarding its settings request
to observing its HTTP response. The restore intervals run from click to readback.

| Time (UTC+8) | Browser action | Observed request or result |
| --- | --- | --- |
| 22:26:45.827–22:26:47.996 | Restore in the original English | English message, zero writes |
| 22:26:51.668–22:26:51.817 | Autosave Simplified Chinese | `PUT /api/settings`, HTTP 200 |
| 22:26:53.890–22:26:55.970 | Restore in Simplified Chinese | Chinese message, zero writes |
| 22:26:56.404–22:26:56.628 | Autosave the return to English | `PUT /api/settings`, HTTP 200 |

After each restore window the test client requested a write and a read through
`POST /api/knx/command`. Each request returned HTTP 202. The peer captured
the panel's GroupValueWrite and GroupValueRead at `30/7/255` and supplied the
GroupValueResponse listed above. The 80% write appeared on the wire at
22:26:48.473. The 0% write appeared at 22:26:58.898, after the editor had
returned to English. The panel marked each reply as live feedback. Its transmit
counter advanced from 0 to 2 to 4 and its receive counter from 0 to 1 to 2.

## Restoration and scope

The original 76-object registry resumed routing. Its export matched the
pre-flash SHA-256, `07ed6611a32e06e4f52dd65c094b4b1192e9dece712d63a578d8981b86bc1b4b`.
The 10-page, 61-widget layout stayed in place and matched the pre-flash bytes,
including the **Light Angle** and **Power history** bindings.
Accounts, cameras, language and theme catalogs, and saved settings matched.
English, Glow, Asia/Shanghai, the 12-hour flip clock and the 23:00 night start
were retained. Only the live clock and Wi-Fi signal differed in the settings
export. Signing out of the test session returned HTTP 401 for the protected layout.

The 144.790-second serial capture contained one application boot. At the final
API check, uptime was 123 seconds. The reboot log still showed boot 270,
the running slot was `ota_0`, the image was valid, OTA was idle and the crash
inventory was empty. These observations showed no rollback during the run.
The KNX peer was simulated. Physical household actuators and Home Assistant
history were outside this test.
