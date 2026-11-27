<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 -->

# Panel10 KNX registry capacity validation

The first November 27, 2026 trial used the Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1
LCD-X on COM24. The original `c9d61c30` firmware had 76 KNX objects and
10 dashboard pages with 61 widgets, including the **Light Angle** and **Power history** graphs.

## Backup and application

The administrator exported the layout, KNX registry and state, settings, accounts,
themes, camera configuration and diagnostics. The raw LittleFS backup was
19,529,728 bytes. It mounted with eight files. The saved layout agreed with the
web export after excluding the three display geometry fields added by the API.
The stored KNX registry matched the export byte for byte. The crash inventory was
empty. The backup contained no `.grph` or `.grph.tmp` files for a sample-level history comparison.

The first pre-write check opened an older reference image and stopped the
procedure. The original firmware restarted. Its application and ELF hashes
matched the archived `c9d61c30` build. The repeated backup checked the original
data and running application again before the write.

| Artifact | SHA-256 |
| --- | --- |
| Application, 6,813,520 bytes | `5c96f28580bf4bc9451532e2bac89de9a1a3a34457586a7b8430433f964c9e25` |
| Matching ELF | `194c7df6852f2543bf599fbe8a43c0b44479656427dd8e3b71e84c183477ee84` |
| Entire `ota_0` read back on COM24 | `74556f7bf207270a27f08b045e9254f73d0c5538a18c8649ab486e0d87c3eee4` |

The ESP-IDF 6.1.1 workspace image is based on `bdc15576-dirty`. Its panel build
stamp is **Nov 26 2026 23:52:05**. Its ESP-IDF application descriptor records
**23:31:47**. The application fits the existing OTA slot with 2,224 bytes to spare.
The serial write covered `0x020000–0x69ffff` in `ota_0`. An independent read of
all 6,815,744 bytes reproduced the pinned application followed by the 2,224
original tail bytes. Before the new application booted, the inactive `ota_1`,
metadata including NVS and OTA selection, crash partition and LittleFS matched
their pre-write digests.

The 100.018-second COM24 recording starts at the final reset. The bootloader
selected `0x20000` at 1.317 s, logged ELF prefix `194c7df68` at 2.791 s,
opened HTTP at 22.856 s and loaded the original 76 KNX objects at 24.903 s.
The splash closed at 25.536 s. The authenticated API reported v0.9.2,
`bdc15576-dirty`, **23:52:05**, valid `ota_0`, next `ota_1` and idle OTA.
The editor JavaScript served by the panel matched the copy embedded in the image.

## Isolated registry

The panel and a host-simulated KNXnet/IP Routing peer used
`239.255.71.179:43674`. The fixture assigned 512 unique IDs to 512 unique
group addresses. The last object, `qa_cap_511` on `30/7/255`, was a readable
and writable DPT 5.001 percentage. The other 511 objects were read-only.

Chrome loaded the editor directly from the panel. It imported a 144,435-byte
JSON file, saved it, and listed all 512 objects again after a browser reload.
COM24 recorded a 106,494-byte registry write to LittleFS in 1,276 ms.
Stopping and restarting KNX routing kept all IDs and addresses in order.
The authenticated objects API returned 26 pages, ending with 12 objects at
offset 500.

| Device exercise | Observed result |
| --- | --- |
| Import 513 objects in the editor | The editor displayed “KNX request failed: Invalid or oversized KNX configuration/value” and kept the 512 saved objects. |
| Submit the same 513 objects directly to the authenticated API | HTTP 400 with `Too many KNX communication objects`. The 106,494-byte saved registry was byte-identical before and after. Its SHA-256 was `816d6b717d4e3166438c9fd3d0e5bb16eca92185618098d5a76e7dc44e0f6267`. Registry generation 8 and routing epoch 9 were unchanged. |
| Bind the last object after rejection | The editor fetched the page at offset 500 and saved a temporary slider with `qa_cap_511` as both brightness command and feedback. All ten original pages were preserved. |
| Read the object | The peer captured a GroupValueRead from the panel on `30/7/255`. Its GroupValueResponse with raw 64 appeared in the panel cache as 25.10%. |
| Send 80% using the editor | The API returned HTTP 202. The peer then captured the panel's GroupValueWrite with raw 204 on the same address. |
| Operate the native slider through Live Display | A single tap produced no captured write. After restoring the original registry and reinstalling the isolated fixture, a recorded drag and release issued 61%. The panel's sent counter advanced from 2 to 3, and the peer captured raw 156 on `30/7/255`. |
| Send fresh feedback from the peer | A GroupValueResponse with raw 158 produced 61.96% in the panel cache. The native slider and the re-opened editor preview both displayed **62%**. |

![Panel screenshot of the bound Registry point 512 slider showing 62 percent](../img/development/panel10-knx-registry-512.png)

![Reopened editor preview of Registry point 512 showing 62 percent](../img/development/panel10-knx-registry-512-editor.png){ width="302" }

The two captures above are from the **23:52:05** build. The read, the editor's
80% write and the 62% screenshot belong to the same routing session after the
513-object rejection. The peer received the read and raw 204 write before the
original registry was restored. The later drag provides separate evidence for
the native control.

At 512 objects with routing active, internal heap had 138,155 bytes free and
an 83,968-byte largest free block. This sample includes the running dashboard
and web services. The 23:52:05 test used browser reload and a routing restart.
The [later power cycle](#power-cycle-with-512-stored-objects) covers startup with
512 objects already stored. Both trials used an isolated simulated KNX peer.

## Restoration

After removing the temporary page and reloading the original registry, the
panel was routing with 76 objects. The final layout and registry exports matched
their pre-flash copies byte for byte. The layout SHA-256 was
`b14c3c3dc9adb56498241a024326ff785485a2196237b0f7d967de137043f196`.
The registry SHA-256 was `07ed6611a32e06e4f52dd65c094b4b1192e9dece712d63a578d8981b86bc1b4b`.
Both original graphs retained their IDs and bindings.

Accounts, themes, camera configuration and languages also matched their
exports. Only the live clock and Wi-Fi signal differed in the settings export.
English, Glow, Asia/Shanghai, 12-hour time, the flip clock and the 23:00 night
start were preserved. At uptime 381 s the boot count was still 266
(264 before the trial, 265 after the pre-write stop). The test session signed
out and anonymous layout access returned HTTP 401.

## Local pre-commit checks

The production parser accepted the archived 76-object configuration, a 128-object
configuration in the old format, and the 352- and 512-object fixtures. The native
service loaded 512 objects from simulated storage. Injecting an allocation failure
at each step of the last two-object page, including a composite value and widget
roles, returned an error with all page allocations released. A subsequent page
read succeeded. The browser rejected a short final page and accepted a complete retry.
In a 352-object replacement, feedback reached two aliases at opposite ends of
the file while the objects API retained the configured order.

In an isolated Chromium editor session, a 512-object backup downloaded successfully.
Restoring a 513-object backup over a 128-object registry failed on the first write
and preserved the old registry. Restoring the 512-object backup then reloading
reproduced every object in order. A legacy backup without a KNX section preserved
all 512. The local service used a synthetic dashboard with no active custom theme.

All 13 CTest cases, 696 editor tests and 19 local-server tests passed.
The Panel10 SDL run passed 273 KNX control checks and 388 general control checks.
At the time of this pre-commit review, the **23:52:05** image had run on COM24.
The allocation-failure and full-backup restore tests in this section ran on the host.

Both local builds used ESP-IDF 6.1.1 and the reviewed changes on `a76324f`.
The `panels3` build started from the tracked dependency lock in an
isolated checkout. The component manager regenerated the pending lock exactly,
updating the IDF entry and four graphics or compression libraries. The KNX
transport kept the same component hash and pinned revision `19059496`.

| Image | Application bytes | Smallest application partition | Free bytes |
| --- | ---: | ---: | ---: |
| Panel10, COM24 at 23:52:05 | 6,813,520 | 6,815,744 | 2,224 |
| Panel10, local Nov 27 at 19:52:10 | 6,814,032 | 6,815,744 | 1,712 |
| PanelS3, local Nov 27 at 19:46:33 | 4,645,888 | 6,815,744 | 2,169,856 |

## Cold-start and backup-restore follow-up for `8b423af`

The November 27 follow-up began on COM24 with the **23:52:05** image running
in `ota_0` at boot 266. Chrome downloaded a 62,731-byte panel backup.
Its layout (10 pages, 61 widgets), 76-object KNX registry, settings and camera
configuration agreed with the individual exports. The crash inventory was empty.
Event times below use the recording host's clock (UTC+8).

### Application readback

The clean `8b423afc` build used ESP-IDF 6.1.1. The embedded C6 3.1.0 adapter
decompressed to the reference SHA-256. The **Nov 27 2026 20:33:45**
application occupied 6,814,640 bytes, leaving **1,104 bytes** in the slot.

| Artifact | SHA-256 |
| --- | --- |
| Application | `99bb6e6eb9a8bbc39d8fd67a6cbe1d3e5cc44b9fca9808461a84712856fc75cc` |
| ELF matched by the application descriptor | `eb4f7133ca8322cf86d81252d085708446f8a0105d534af68fabd7c356a635d3` |
| Entire `ota_0` read back on COM24 | `cf7aecde70254a1f43943cc87c27e0cbdc007f6547bac9ea75cffd4e0e09f5d3` |

The backups of the old application, metadata, crash partition and
19,529,728-byte LittleFS matched the device digests. LittleFS mounted with
eight files. Its stored registry matched the web export byte for byte.
Its layout matched after accounting for three geometry fields added by the API.

The write covered only `ota_0` at `0x20000`. All 6,815,744 bytes were then
read independently over COM24. They matched the new application followed by
the original 1,104 tail bytes. Before reset, the inactive `ota_1`, metadata
(including NVS and OTA selection), crash partition and LittleFS matched their
pre-write digests. COM24 recorded the bootloader selecting `0x20000` at
1.318 s from reset, the matching ELF prefix at 2.791 s, the original 76
objects at 20.376 s and splash closure at 21.018 s. The authenticated
API identified boot 267, v0.9.2, `8b423afc`, **20:33:45**, valid `ota_0`, next
`ota_1` and idle OTA.

### Power cycle with 512 stored objects

The device-hosted editor saved the isolated fixture and listed all 512
objects after reload. COM24 logged a 106,494-byte registry write in 1,178 ms.
Before power removal, the objects API returned 26 pages in configured order,
including `qa_cap_511` at `30/7/255` on the last page.

The operator disconnected power for at least ten seconds and then reconnected it.
The intervals between sampled loss and return were 16.501 s for COM24 and
32.783 s for HTTP, including service startup. The reboot record advanced from
267 to **268** with reason `power-on`. COM24 recorded
`Configuration: valid, objects=512, enabled=1` during startup.
Before any post-boot registry write, the authenticated API
returned the same 106,494 bytes (SHA-256
`816d6b717d4e3166438c9fd3d0e5bb16eca92185618098d5a76e7dc44e0f6267`).
All 512 IDs and group addresses were in order across 26 pages at registry
generation 1 and routing epoch 1. At offset 500 the panel returned the final
12 objects. Point 512 was readable and writable and had no cached feedback.
The bootloader, running version and OTA state again identified the verified
`ota_0` image.

On that cold-loaded registry, the editor appended a temporary page to the
ten original pages and bound both slider roles to `qa_cap_511` (DPT 5.001).
The isolated peer captured a GroupValueRead on `30/7/255` and replied with
raw 64 (25.10%). An 80% write from the editor's KNX settings received HTTP 202
when queued. The peer then captured the panel's GroupValueWrite with raw 204.
A drag on the native slider through Live Display produced raw 156 (61%),
and the panel's sent count advanced from 2 to 3.

### Oversized backup and restoration

A host logging error after the native drag triggered restoration of the
76-object registry and original layout. Both oversized-backup attempts ran
later on boot **268**. Each started by reloading the same 512-object
registry and binding the temporary slider again.

The first restore returned HTTP 400 at 20:52:12. The browser check timed out
waiting for the server's detailed reason in the visible message. Cleanup
restored the original registry and layout.

Before the final restore, the simulated actuator returned raw 158 and the
panel reported 61.96%. The native LVGL screenshot and web preview both showed
**62%** at this point. Chrome then downloaded the full panel backup. A copy
with only the 513th KNX object added was selected in the editor. Confirming
**Restore** sent one write, `PUT /api/knx/config`. At 20:55:15 COM24 recorded
HTTP 400. The response body was `Too many KNX communication objects`. The
editor displayed only “Backup failed: Request failed. Check the connection and try again.”

Immediately after rejection, the 512-object registry and layout with the
temporary slider were byte-identical to their pre-restore exports. Registry
generation stayed at 5 and routing epoch stayed at 9. On that same routing
session the peer captured a fresh write of raw 0 and the panel accepted its
0% feedback. The next GroupValueRead received raw 158, which the object API
again reported as 61.96%.

After the test, the original 76-object registry resumed routing. The final
layout (10 pages, 61 widgets) and registry matched the pre-flash exports
byte for byte. Their hashes are listed under [Restoration](#restoration).
**Light Angle** and **Power history** kept their IDs and bindings. The
pre-flash LittleFS contained no `.grph` or `.grph.tmp` files, so individual
history samples could not be compared. Accounts, cameras, themes, language
data and saved settings matched the backups. Only the live time and Wi-Fi signal
changed in the settings export. English, Glow, Asia/Shanghai, 12-hour time,
the flip clock and the 23:00 night start remained selected. At uptime 417 s
the device was still on boot 268 with valid `ota_0`, idle OTA and zero
retained crash reports. Signing out returned HTTP 401 for the protected layout.

## Local verification of the backup limit message, November 27, 2026

On COM24, the `8b423afc` image returned a specific HTTP 400 reason. The editor
prefixed it with the request path and its translation filter substituted the
generic connection error. The revised editor checks the backup before asking
for confirmation and recognizes the panel's capacity error on the KNX registry endpoint.

Chrome 158 on Windows loaded the HTML, CSS and JavaScript embedded in the new
image against a local API with 512 synthetic objects and a slider bound to the
last one. Restoring a modified backup with 513 objects,
KNX disabled and an empty layout showed the 512-object limit in all seven built-in
languages. There were zero confirmation dialogs and zero write requests. The
registry, enable setting and bound layout matched the pre-test exports. A direct
oversized PUT to the local API also returned HTTP 400 and preserved those exports.

The web regression sent an 80% command and a read for object 512 after rejection.
The KNX settings cache view showed independently supplied feedback of 62 (DPT 5.001).
In a separate native test, the production KNX worker rejected 513 objects,
sent an off telegram to `1/1/255` and accepted a fresh false response from
the simulated peer. These checks used local transports. The following COM24
run exercised the same revised editor on the panel.

The build-time run passed 698 web tests, 13 native CTest cases and 19 local API tests.
The pre-commit run passed 699 web tests, including refusal with a pending language
autosave, plus the same 13 native and 19 local API tests.
ESP-IDF 6.1.1 built `panel10` from `53f5d07e-dirty` at **21:25:04**.
The application occupied **6,814,400 of 6,815,744 bytes**, leaving **1,344 bytes**
in either OTA slot. The previous local image had 1,712 bytes free and the
clean COM24 image had 1,104. The application checksum, matching ELF digest
and embedded C6 3.1.0 adapter were verified. The application SHA-256 is
`6c31bd63843ebc43a258f220197f6cf25f13c6dac9d4556e2e89cc4ba886f1f0`.

## COM24 restore limit in English and Simplified Chinese, November 27, 2026

The 62,731-byte editor backup agreed with the independent device exports. COM24
verified the old application, metadata, crash partition and 19,529,728-byte LittleFS
before writing only `ota_0` at `0x20000`. Its independent 6,815,744-byte readback
matched the **21:25:04** application above plus the original 1,344 tail bytes.
The whole-slot SHA-256 was `21e6e56d5b6e27d479d9392c9f04253003c8ee1f9fc66e05f240764780175852`.
The inactive slot and protected regions matched their pre-write digests. Serial
recorded the expected boot address and ELF prefix `b0f5d0657`. Boot 269 reported
`53f5d07e-dirty`, valid `ota_0` and idle OTA. The 367.1-second capture showed one boot.

Chrome loaded the editor from the panel. Both clicks on **Restore backup** used the
same downloaded file with only object 513 appended. Each produced its localized
message with zero confirmation dialogs and zero write requests through readback.
An earlier trace included a language autosave. In the completed run (UTC+8), the
Chinese `PUT /api/settings` finished at 21:48:58.547, the Chinese restore was observed
from 21:49:00.931 to 21:49:02.988, and the return to English finished at 21:49:03.606.
Both language saves returned HTTP 200.

| Editor language | Visible restore message | Next write on object 512 | Reply to the next read |
| --- | --- | --- | --- |
| English | A KNX registry supports at most 512 communication objects. | 80% (raw 204) | Raw 158, shown by the API as 61.96% |
| Simplified Chinese | KNX 注册表最多支持 512 个通信对象。 | 0% (raw 0) | Raw 64, shown by the API as 25.10% |

Both refusals preserved the byte-identical layout and 512-object registry at generation 6,
routing epoch 11. The isolated peer captured each write and read on `30/7/255` and
the panel marked both independent replies as live feedback. The original 76-object
registry resumed routing. All 10 pages and 61 widgets matched the pre-flash layout.
Accounts, saved settings, cameras, languages and themes matched. Only the live clock
advanced. Signing out returned HTTP 401 for the layout. **Light Angle** and **Power history**
kept their bindings. The pre-flash LittleFS had no local graph files for a sample comparison.
The [clean 5ca8249 build](panel10-knx-backup-restore-validation.md) has a separate COM24 record.
